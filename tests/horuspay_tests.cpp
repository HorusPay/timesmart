#include <boost/test/unit_test.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <eosio/chain/permission_object.hpp>
#include <eosio/chain/trace.hpp>
#include <cstdlib>
#include <iostream>
#include <array>
#include <utility>
#include <fc/log/logger.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/signature.hpp>
#include <eosio/chain/exceptions.hpp>
#include <Runtime/Runtime.h>

#include "eosio.system_tester.hpp"

using namespace eosio_system;
using namespace eosio;
using namespace std;
using namespace fc::crypto;

using eosio::chain::action_trace;

const static account_name ME = account_name("horuspay");
const static symbol core_symbol = symbol{CORE_SYM};
const static name system_account_name = eosio::chain::config::system_account_name;

struct eosio_assert_message_is_log {
   eosio_assert_message_is_log( const string& msg )
         : expected( "assertion failure with message: " ) {
      expected.append( msg );
      std::cout << "eosio_assert_message_is_log (contructor): " << msg << std::endl;
   }

   bool operator()( const eosio_assert_message_exception& ex ) {
      std::cout << "operator(): [" << ex.to_string() << "]|[" << expected << "]" << std::endl;
      return ex.to_string() == expected;
   }

   string expected;
};


struct project {
   name                     name;
   bool                     need_approval;
   optional<extended_asset> hourly_rate;
   optional<extended_asset> balance;
};
FC_REFLECT( project, (name)(need_approval)(hourly_rate)(balance));

struct project_user {
   uint64_t          id;
   name              project;
   name              user;
   int64_t           approved;
   int64_t           pending;
   block_timestamp_type   last_clock;
};
FC_REFLECT( project_user, (id)(project)(user)(approved)(pending)(last_clock));

struct project_manager {
   uint64_t id;
   name     project;
   name     manager;
   bool     is_owner;
};
FC_REFLECT( project_manager, (id)(project)(manager)(is_owner));


struct horuspay_tester : eosio_system_tester {
   
   abi_serializer horuspay_abi; 

   bool print_console = false;
   void set_print_console(bool value) {
      print_console = value;
   }

   void print_debug(const action_trace& ar) {
      if (!ar.console.empty()) {
         cout << ": CONSOLE OUTPUT BEGIN =====================" << endl
            << ar.console << endl
            << ": CONSOLE OUTPUT END   =====================" << endl;
      }
      // for(const auto& it : ar.inline_traces) {
      //    print_debug(it);
      // }
   }

   horuspay_tester() {

      create_account_with_resources(ME, system_account_name, 1500000);
      transfer(system_account_name, ME, core_sym::from_string("100.0000"));

      // // const auto& db  = control->db();
      // // auto me_active = db.get<permission_object, eosio::chain::by_owner>( boost::make_tuple(ME, name("active")) );

      auto trace_auth = TESTER::push_action(system_account_name, updateauth::get_name(), ME, mvo()
                                            ("account", ME)
                                            ("permission", name("active"))
                                            ("parent", name("owner"))
                                            ("auth",authority(1,{ key_weight{get_public_key( ME, "active" ), 1}}, {permission_level_weight{{ME, name("eosio.code")}, 1}})
                                            )
      );
      // BOOST_REQUIRE_EQUAL(transaction_receipt::executed, trace_auth->receipt->status);
      produce_block();

      set_code( ME, contracts::horuspay_wasm());
      set_abi( ME, contracts::horuspay_abi().data() );

      const auto& accnt = control->db().get<account_object,by_name>(ME);
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      horuspay_abi.set_abi(abi, abi_serializer_max_time);
   }
   
   transaction_trace_ptr last_tx_trace;
   typename base_tester::action_result my_push_action(action&& act, uint64_t authorizer) {
      signed_transaction trx;
      if (authorizer) {
         act.authorization = vector<permission_level>{{authorizer, name("active")}};
      }
      trx.actions.emplace_back(std::move(act));
      set_transaction_headers(trx);
      if (authorizer) {
         trx.sign(get_private_key(authorizer, "active"), control->get_chain_id());
      }
      try {
         last_tx_trace = push_transaction(trx);
         if(print_console) {
            print_debug(last_tx_trace->action_traces[0]);
         }
      } catch (const fc::exception& ex) {
         if(print_console) {
            cout << "-----EXCEPTION------" << endl
                 << fc::json::to_string(ex) << endl;
         }
         edump((ex));
         edump((ex.to_detail_string()));
         return error(ex.top_message()); // top_message() is assumed by many tests; otherwise they fail
      }
      produce_block();
      BOOST_REQUIRE_EQUAL(true, chain_has_transaction(trx.id()));
      return success();
   }

   action_result call( const account_name& signer, const action_name &name, const variant_object &data ) {
         
      string action_type_name = horuspay_abi.get_action_type(name);

      action act;
      act.account = ME;
      act.name    = name;
      act.data    = horuspay_abi.variant_to_binary( action_type_name, data, abi_serializer_max_time );
      return my_push_action(std::move(act), signer);
   }

   action_result create(account_name project, account_name owner, optional<extended_asset> hourly_rate) {
      return call(owner, N(create),mvo()
         ("project",     project)
         ("owner",       owner)
         ("hourly_rate", hourly_rate)
      );
   }

   action_result adduser(account_name project, account_name manager, account_name user) {
      return call(manager, N(adduser), mvo()
         ("project",  project)
         ("manager",  manager)
         ("user",     user)
      );
   }

   action_result removeuser(account_name project, account_name manager, account_name user) {
      return call(manager, N(removeuser), mvo()
         ("project",  project)
         ("manager",  manager)
         ("user",     user)
      );
   }

   action_result addmanager(account_name project, account_name owner, account_name manager) {
      return call(owner, N(addmanager), mvo()
         ("project", project)
         ("owner",   owner)
         ("manager", manager)
      );
   }

   action_result rmvmanager(account_name project, account_name owner, account_name manager) {
      return call(owner, N(rmvmanager), mvo()
         ("project", project)
         ("owner",   owner)
         ("manager", manager)
      );
   }

   action_result clockin(account_name project, account_name user) {
      return call(user, N(clockin), mvo()
         ("project", project)
         ("user",    user)
      );
   }

   action_result clockout(account_name project, account_name user, optional<string> description) {
      return call(user, N(clockout), mvo()
         ("project",     project)
         ("user",        user)
         ("description", description)
      );
   }

   action_result addtime(account_name project, account_name user, uint64_t hours, optional<string> description) {
      return call(user, N(addtime), mvo()
         ("project",     project)
         ("user",        user)
         ("hours",       hours)
         ("description", description)
      );
   }

   action_result approve(account_name project, account_name manager, account_name user, optional<int64_t> hours) {
      return call(manager, N(approve), mvo()
         ("project",     project)
         ("manager",     manager)
         ("user",        user)
         ("hours",       hours)
      );
   }

   action_result claim(account_name project, account_name user, optional<int64_t> hours) {
      return call(user, N(claim), mvo()
         ("project",     project)
         ("user",        user)
         ("hours",       hours)
      );
   }

   void transfer_with_memo( name from, name to, const asset& amount, const string& memo = "", name token_contract=N(eosio.token) ) {
      last_tx_trace = base_tester::push_action( token_contract, N(transfer), from, mutable_variant_object()
                                ("from",     from)
                                ("to",       to )
                                ("quantity", amount)
                                ("memo",     memo)
                                );
      print_debug(last_tx_trace->action_traces[0]);
   }

   optional<project> get_project(const account_name& prjname) {
      vector<char> data = get_row_by_account( ME, ME, N(project), prjname );
      if( data.empty() )
         return {};
      std::cout << "get_row_by_account: " << fc::to_hex(data) << std::endl;
      return horuspay_abi.binary_to_variant("project", data, abi_serializer_max_time).as<project>();
   }

   optional<project_manager> get_project_manager(uint64_t id) {
      vector<char> data = get_row_by_account( ME, ME, N(projectmgr), id );
      if( data.empty() )
         return {};

      std::cout << "get_project_manager: " << fc::to_hex(data) << std::endl;
      return horuspay_abi.binary_to_variant("project_manager", data, abi_serializer_max_time).as<project_manager>();
   }

   optional<project_user> get_project_user(uint64_t id) {
      vector<char> data = get_row_by_account( ME, ME, N(projectuser), id );
      if( data.empty() )
         return {};

      std::cout << "get_project_user: " << fc::to_hex(data) << std::endl;
      return horuspay_abi.binary_to_variant("project_user", data, abi_serializer_max_time).as<project_user>();
   }

   asset get_internal_balance( const account_name& act, symbol balance_symbol = symbol{CORE_SYM} ) {
      vector<char> data = get_row_by_account( ME, act, N(accounts), balance_symbol.to_symbol_code().value );
      return data.empty() ? asset(0, balance_symbol) : horuspay_abi.binary_to_variant("account", data, abi_serializer_max_time)["balance"].as<asset>();
   }

};

BOOST_AUTO_TEST_SUITE(horuspay_tests)

BOOST_FIXTURE_TEST_CASE( test_all, horuspay_tester ) try {

   create_account_with_resources(N(user1), system_account_name);
   create_account_with_resources(N(user2), system_account_name);
   create_account_with_resources(N(user3), system_account_name);

   create_account_with_resources(N(own1), system_account_name);
   create_account_with_resources(N(own2), system_account_name);
   create_account_with_resources(N(mgr1), system_account_name);
   create_account_with_resources(N(mgr2), system_account_name);
   

   create_currency(name("eosio.token"), system_account_name, asset::from_string("100000.0000 USD"));
   create_currency(name("eosio.token"), system_account_name, asset::from_string("100000.0000 EUR"));
   create_currency(name("eosio.token"), system_account_name, asset::from_string("100000.0000 ARS"));

   //Fake USD
   create_account_with_resources(N(faketoken), system_account_name);
   buyram( "eosio", "faketoken", core_sym::from_string("5000.0000") );
   set_code( N(faketoken), contracts::token_wasm());
   set_abi( N(faketoken), contracts::token_abi().data() );
   create_currency(N(faketoken), system_account_name, asset::from_string("100000.0000 USD"));


   // Create project
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Hourly rate must be positive")
      , create(N(proj1), N(own1), extended_asset(asset::from_string("0.0000 USD"), N(eosio.token))));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("Invalid token account")
      , create(N(proj1), N(own1), extended_asset(asset::from_string("10.0000 USD"), N(eosio.taken))));

   BOOST_REQUIRE_EQUAL( success()
      , create(N(proj1), N(own1), extended_asset(asset::from_string("10.0000 USD"), N(eosio.token))));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("A project with that name already exists")
      , create(N(proj1), N(own1), extended_asset(asset::from_string("10.0000 USD"), N(eosio.token))));

   auto prj = get_project(N(proj1));
   BOOST_REQUIRE(!!prj);

   BOOST_REQUIRE_EQUAL(prj->name, N(proj1));
   BOOST_REQUIRE_EQUAL(prj->need_approval, true);
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->quantity, asset::from_string("10.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->contract, N(eosio.token));
   BOOST_REQUIRE_EQUAL(prj->balance->quantity, asset::from_string("0.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->balance->contract, N(eosio.token));



   // Add user
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only project manager can add users")
      , adduser(N(xxx), N(own1), N(own1)));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only project manager can add users")
      , adduser(N(proj1), N(user1), N(user2)));

   BOOST_REQUIRE_EQUAL( success()
      , adduser(N(proj1), N(own1), N(user1)));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("the user is already a member of the project")
      , adduser(N(proj1), N(own1), N(user1)));

   BOOST_REQUIRE_EQUAL( success()
      , adduser(N(proj1), N(own1), N(user2)));
   
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("user must be a registered account")
      , adduser(N(proj1), N(own1), N(user11)));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only project admins can remove users")
      , removeuser(N(proj1), N(user2), N(user2)));

   BOOST_REQUIRE_EQUAL( success()
      , removeuser(N(proj1), N(own1), N(user2)));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("the user is not member of the project")
      , removeuser(N(proj1), N(own1), N(user3)));

   auto prjmgr = get_project_manager(uint64_t(0));
   BOOST_REQUIRE(!!prjmgr);
   BOOST_REQUIRE_EQUAL(prjmgr->project, N(proj1));
   BOOST_REQUIRE_EQUAL(prjmgr->manager, N(own1));
   BOOST_REQUIRE_EQUAL(prjmgr->is_owner, true);

   // Add manager
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("manager is already a manager of the project")
      , addmanager(N(proj1), N(own1), N(own1)));

   BOOST_REQUIRE_EQUAL( success()
      , addmanager(N(proj1), N(own1), N(mgr1)));
   
   prjmgr = get_project_manager(uint64_t(1));
   BOOST_REQUIRE(!!prjmgr);
   BOOST_REQUIRE_EQUAL(prjmgr->project, N(proj1));
   BOOST_REQUIRE_EQUAL(prjmgr->manager, N(mgr1));
   BOOST_REQUIRE_EQUAL(prjmgr->is_owner, false);

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only project owner can add new managers")
      , addmanager(N(proj1), N(mgr1), N(mgr2)));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only project owner can add new managers")
      , addmanager(N(proj1), N(user1), N(mgr2)));

   BOOST_REQUIRE_EQUAL( success()
      , addmanager(N(proj1), N(own1), N(mgr2)));

   prjmgr = get_project_manager(uint64_t(2));
   BOOST_REQUIRE(!!prjmgr);
   BOOST_REQUIRE_EQUAL(prjmgr->project, N(proj1));
   BOOST_REQUIRE_EQUAL(prjmgr->manager, N(mgr2));
   BOOST_REQUIRE_EQUAL(prjmgr->is_owner, false);

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("manager must be a registered account")
      , addmanager(N(proj1), N(own1), N(mgr3)));

   // Remove manager
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only project owner can remove managers")
      , rmvmanager(N(proj1), N(mgr1), N(mgr2)));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only project owner can remove managers")
      , rmvmanager(N(proj1), N(mgr1), N(mgr3)));

   BOOST_REQUIRE_EQUAL( success()
      , rmvmanager(N(proj1), N(own1), N(mgr2)));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("not a manager of the project")
      , rmvmanager(N(proj1), N(own1), N(mgr2)));

   // Clock in
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("the user is not a member of the project")
      , clockin(N(proj1), N(user3)));
   
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("must clockin first")
      , clockout(N(proj1), N(user1), {}));

   BOOST_REQUIRE_EQUAL( success()
      , clockin(N(proj1), N(user1)));

   auto prjusr = get_project_user(uint64_t(0));
   BOOST_REQUIRE(!!prjusr);
   BOOST_REQUIRE_EQUAL(prjusr->id,0);
   BOOST_REQUIRE_EQUAL(prjusr->project,N(proj1));
   BOOST_REQUIRE_EQUAL(prjusr->user,N(user1));
   BOOST_REQUIRE_EQUAL(prjusr->approved,0);
   BOOST_REQUIRE_EQUAL(prjusr->pending,0);
   BOOST_REQUIRE_EQUAL(prjusr->last_clock.slot, block_timestamp_type(control->head_block_time()).slot);
   
   produce_block( fc::hours(2) );

   BOOST_REQUIRE_EQUAL( success()
      , clockout(N(proj1), N(user1), {}));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("must clockin first")
      , clockout(N(proj1), N(user1), {}));

   prjusr = get_project_user(uint64_t(0));

   BOOST_REQUIRE(!!prjusr);
   BOOST_REQUIRE_EQUAL(prjusr->id,0);
   BOOST_REQUIRE_EQUAL(prjusr->project,N(proj1));
   BOOST_REQUIRE_EQUAL(prjusr->user,N(user1));
   BOOST_REQUIRE_EQUAL(prjusr->approved,0);
   BOOST_REQUIRE_EQUAL(prjusr->pending, 2*60*60);
   BOOST_REQUIRE_EQUAL(prjusr->last_clock.slot, 0);

   produce_block( fc::hours(1) );
   BOOST_REQUIRE_EQUAL( success()
      , clockin(N(proj1), N(user1)));

   produce_block( fc::hours(1) );
   BOOST_REQUIRE_EQUAL( success()
      , clockin(N(proj1), N(user1)));

   produce_block( fc::hours(3) );
   BOOST_REQUIRE_EQUAL( success()
      , clockout(N(proj1), N(user1), {}));
   
   prjusr = get_project_user(uint64_t(0));
   BOOST_REQUIRE(!!prjusr);
   BOOST_REQUIRE_EQUAL(prjusr->id,0);
   BOOST_REQUIRE_EQUAL(prjusr->project,N(proj1));
   BOOST_REQUIRE_EQUAL(prjusr->user,N(user1));
   BOOST_REQUIRE_EQUAL(prjusr->approved,0);
   BOOST_REQUIRE_EQUAL(prjusr->pending, 5*60*60);
   BOOST_REQUIRE_EQUAL(prjusr->last_clock.slot, 0);

   // Add hours
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("hours must be positive")
      , addtime(N(proj1), N(user1), 0, {}));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("the user is not a member of the project")
      , addtime(N(proj1), N(user3), 2, {}));

   BOOST_REQUIRE_EQUAL( success()
      , addtime(N(proj1), N(user1), 1, "work on xxx"));

   BOOST_REQUIRE_EQUAL( success()
      , addtime(N(proj1), N(user1), 6, "work on yyy"));

   prjusr = get_project_user(uint64_t(0));
   BOOST_REQUIRE(!!prjusr);
   BOOST_REQUIRE_EQUAL(prjusr->id,0);
   BOOST_REQUIRE_EQUAL(prjusr->project,N(proj1));
   BOOST_REQUIRE_EQUAL(prjusr->user,N(user1));
   BOOST_REQUIRE_EQUAL(prjusr->approved,0);
   BOOST_REQUIRE_EQUAL(prjusr->pending, (5+7)*60*60);
   BOOST_REQUIRE_EQUAL(prjusr->last_clock.slot, 0);

   // Approve & claim
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only managers can approve hours")
   , approve(N(proj1), N(user1), N(user1),{}));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("only managers can approve hours")
   , approve(N(proj1), N(mgr2), N(user1),{}));

   BOOST_REQUIRE_EQUAL( success()
   , approve(N(proj1), N(mgr1), N(user1),1));

   prjusr = get_project_user(uint64_t(0));
   BOOST_REQUIRE(!!prjusr);
   BOOST_REQUIRE_EQUAL(prjusr->id,0);
   BOOST_REQUIRE_EQUAL(prjusr->project,N(proj1));
   BOOST_REQUIRE_EQUAL(prjusr->user,N(user1));
   BOOST_REQUIRE_EQUAL(prjusr->approved,(1)*60*60);
   BOOST_REQUIRE_EQUAL(prjusr->pending, (5+7-1)*60*60);
   BOOST_REQUIRE_EQUAL(prjusr->last_clock.slot, 0);

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("the user is not a member of the project")
   , approve(N(proj1), N(mgr1), N(user3),0));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("0 < approve <= pending")
   , approve(N(proj1), N(mgr1), N(user1),0));

   BOOST_REQUIRE_EQUAL( success()
   , approve(N(proj1), N(mgr1), N(user1), {}));

   prjusr = get_project_user(uint64_t(0));
   BOOST_REQUIRE(!!prjusr);
   BOOST_REQUIRE_EQUAL(prjusr->id,0);
   BOOST_REQUIRE_EQUAL(prjusr->project,N(proj1));
   BOOST_REQUIRE_EQUAL(prjusr->user,N(user1));
   BOOST_REQUIRE_EQUAL(prjusr->approved,(5+7)*60*60);
   BOOST_REQUIRE_EQUAL(prjusr->pending, 0);
   BOOST_REQUIRE_EQUAL(prjusr->last_clock.slot, 0);

   // Claim prj2
   BOOST_REQUIRE_EQUAL( success()
      , create(N(proj2), N(own2), {}));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("unable to claim")
      , claim(N(proj2), N(user1), {}) );

   // Claim prj1
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("0 < claim <= approved")
      , claim(N(proj1), N(user1), 0));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("0 < claim <= approved")
      , claim(N(proj1), N(user1), -1));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("claim project not found")
      , claim(N(proj5), N(user1), {}));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("the user is not a member of the project")
      , claim(N(proj1), N(user3), {}));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("not enough funds")
      , claim(N(proj1), N(user1), 1));

   issue(name("mgr1"), asset::from_string("300.0000 USD"));
   issue(name("mgr1"), asset::from_string("300.0000 ARS"));
   //Issue "fake" USD
   base_tester::push_action( N(faketoken), N(issue), system_account_name, mutable_variant_object()
                              ("to",      name("mgr1") )
                              ("quantity", asset::from_string("500.0000 USD") )
                              ("memo", "")
                              );
   issue(name("mgr2"), asset::from_string("300.0000 USD"));

   BOOST_REQUIRE_EXCEPTION( transfer_with_memo( name("mgr1"), ME, asset::from_string("10.0000 USD"), "noproj" ),
         eosio_assert_message_exception, eosio_assert_message_is( "transfer project not found" ) );

   prj = get_project(N(proj1));
   BOOST_REQUIRE(!!prj);
   BOOST_REQUIRE_EQUAL(prj->name, N(proj1));
   BOOST_REQUIRE_EQUAL(prj->need_approval, true);
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->quantity, asset::from_string("10.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->contract, N(eosio.token));
   BOOST_REQUIRE_EQUAL(prj->balance->quantity, asset::from_string("0.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->balance->contract, N(eosio.token));

   transfer_with_memo( name("mgr1"), ME, asset::from_string("10.0000 USD"), "proj1" );

   prj = get_project(N(proj1));
   BOOST_REQUIRE(!!prj);
   BOOST_REQUIRE_EQUAL(prj->name, N(proj1));
   BOOST_REQUIRE_EQUAL(prj->need_approval, true);
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->quantity, asset::from_string("10.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->contract, N(eosio.token));
   BOOST_REQUIRE_EQUAL(prj->balance->quantity, asset::from_string("10.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->balance->contract, N(eosio.token));

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("not enough funds")
      , claim(N(proj1), N(user1), 2));

   BOOST_REQUIRE_EQUAL( success()
     , claim(N(proj1), N(user1), 1));

   prjusr = get_project_user(uint64_t(0));
   BOOST_REQUIRE(!!prjusr);
   BOOST_REQUIRE_EQUAL(prjusr->id,0);
   BOOST_REQUIRE_EQUAL(prjusr->project,N(proj1));
   BOOST_REQUIRE_EQUAL(prjusr->user,N(user1));
   BOOST_REQUIRE_EQUAL(prjusr->approved,(5+7-1)*60*60);
   BOOST_REQUIRE_EQUAL(prjusr->pending, 0);
   BOOST_REQUIRE_EQUAL(prjusr->last_clock.slot, 0);

   prj = get_project(N(proj1));
   BOOST_REQUIRE(!!prj);
   BOOST_REQUIRE_EQUAL(prj->name, N(proj1));
   BOOST_REQUIRE_EQUAL(prj->need_approval, true);
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->quantity, asset::from_string("10.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->contract, N(eosio.token));
   BOOST_REQUIRE_EQUAL(prj->balance->quantity, asset::from_string("0.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->balance->contract, N(eosio.token));

   BOOST_REQUIRE_EXCEPTION( transfer_with_memo( name("mgr2"), ME, asset::from_string("10.0000 USD"), "proj1" ),
         eosio_assert_message_exception, eosio_assert_message_is( "only project managers can deposit" ) );

   BOOST_REQUIRE_EXCEPTION( transfer_with_memo( name("mgr1"), ME, asset::from_string("10.0000 ARS"), "proj1" ),
         eosio_assert_message_exception, eosio_assert_message_is( "invalid deposit token" ) );

   BOOST_REQUIRE_EXCEPTION( transfer_with_memo( name("mgr1"), ME, asset::from_string("10.0000 USD"), "proj1", name("faketoken") ),
         eosio_assert_message_exception, eosio_assert_message_is( "invalid deposit contract" ) );

   transfer_with_memo( name("mgr1"), ME, asset::from_string("110.0000 USD"), "proj1" );

   prj = get_project(N(proj1));
   BOOST_REQUIRE(!!prj);
   BOOST_REQUIRE_EQUAL(prj->name, N(proj1));
   BOOST_REQUIRE_EQUAL(prj->need_approval, true);
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->quantity, asset::from_string("10.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->contract, N(eosio.token));
   BOOST_REQUIRE_EQUAL(prj->balance->quantity, asset::from_string("110.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->balance->contract, N(eosio.token));

   BOOST_REQUIRE_EQUAL( success()
     , claim(N(proj1), N(user1), {}));

   prjusr = get_project_user(uint64_t(0));
   BOOST_REQUIRE(!!prjusr);
   BOOST_REQUIRE_EQUAL(prjusr->id,0);
   BOOST_REQUIRE_EQUAL(prjusr->project,N(proj1));
   BOOST_REQUIRE_EQUAL(prjusr->user,N(user1));
   BOOST_REQUIRE_EQUAL(prjusr->approved,0);
   BOOST_REQUIRE_EQUAL(prjusr->pending, 0);
   BOOST_REQUIRE_EQUAL(prjusr->last_clock.slot, 0);

   prj = get_project(N(proj1));
   BOOST_REQUIRE(!!prj);
   BOOST_REQUIRE_EQUAL(prj->name, N(proj1));
   BOOST_REQUIRE_EQUAL(prj->need_approval, true);
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->quantity, asset::from_string("10.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->hourly_rate->contract, N(eosio.token));
   BOOST_REQUIRE_EQUAL(prj->balance->quantity, asset::from_string("0.0000 USD"));
   BOOST_REQUIRE_EQUAL(prj->balance->contract, N(eosio.token));

   BOOST_REQUIRE_EQUAL( asset::from_string("120.0000 USD"), get_balance(N(user1), symbol{4,"USD"}));
   BOOST_REQUIRE_EQUAL( asset::from_string("0.0000 USD"), get_balance(ME, symbol{4,"USD"}));

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
