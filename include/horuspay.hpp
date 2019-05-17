#pragma once

#include <string>
#include <utility>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/name.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/fixed_bytes.hpp>

namespace horuspay {

static uint128_t compute_key(uint64_t user, uint64_t project) {
   return ((uint128_t(user) << 64) | uint128_t(project));
}

using std::string;
using std::optional;
using eosio::name;
using eosio::asset;
using eosio::extended_asset;
using eosio::multi_index;
using eosio::indexed_by;
using eosio::const_mem_fun;
using eosio::fixed_bytes;
using eosio::block_timestamp;
using eosio::same_payer;

class [[eosio::contract]] horuspay : public eosio::contract {
   public:
      using contract::contract;

   struct [[eosio::table]] project {
      name           name;
      extended_asset hourly_rate;
      extended_asset balance;

      uint64_t primary_key() const {
         return name.value;
      }

      EOSLIB_SERIALIZE( project, (name)(hourly_rate)(balance))
   };
   typedef multi_index< "project"_n, project >  project_table;


   struct [[eosio::table]] project_user {
      uint64_t                 id;
      name                     project;
      name                     user;
      int64_t                  pending;
      extended_asset           hourly_rate;
      block_timestamp          last_clock;

      uint64_t primary_key() const {
         return id;
      }

      uint128_t by_project_user() const {
         return compute_key(user.value, project.value);
      }

      EOSLIB_SERIALIZE( project_user, (id)(project)(user)(pending)(hourly_rate)(last_clock))
   };
   typedef eosio::multi_index< "projectuser"_n, project_user,
            eosio::indexed_by<"byusr"_n, const_mem_fun<project_user, uint128_t, &project_user::by_project_user>>
            >  project_user_table;


   struct [[eosio::table]] project_manager {
      uint64_t id;
      name     project;
      name     manager;
      bool     is_owner;

      uint64_t primary_key() const {
         return id;
      }

      uint128_t by_project_manager() const {
         return compute_key(manager.value, project.value);
      } 

      EOSLIB_SERIALIZE( project_manager, (id)(project)(manager)(is_owner))
   };
   typedef multi_index< "projectmgr"_n, project_manager,
            indexed_by<"bymgr"_n, const_mem_fun<project_manager, uint128_t, &project_manager::by_project_manager>>
         >  project_manager_table;


      [[eosio::action]]
      void create(name project, name owner, extended_asset hourly_rate);

      [[eosio::action]]
      void adduser(name project, name manager, name user);

      [[eosio::action]]
      void removeuser(name project, name manager, name user);

      [[eosio::action]]
      void addmanager(name project, name owner, name manager);

      [[eosio::action]]
      void rmvmanager(name project, name owner, name manager);

      [[eosio::action]]
      void clockin(name project, name user);

      [[eosio::action]]
      void clockout(name project, name user, optional<string> description);

      [[eosio::action]]
      void addtime(name project, name user, uint64_t seconds, optional<string> description, optional<name> manager);

      [[eosio::action]]
      void approve(name project, name manager, name user, optional<int64_t> seconds);

      [[eosio::action]]
      void decline(name project, name manager, name user, int64_t seconds);

      [[eosio::action]]
      void setuserrate(name project, name manager, name user, extended_asset hourly_rate);

      //HACK: https://github.com/EOSIO/eosio.cdt/issues/497
      [[eosio::on_notify("eosio.token::transfer")]]
      void on_eosio_token_transfer( name from, name to, asset quantity, const std::string& memo ) {
         on_transfer( from, to, quantity, memo );
      }

      [[eosio::on_notify("*::transfer")]]
      void on_transfer( name from, name to, asset quantity, const std::string& memo );

      struct token {
         void transfer( name         from,
                        name         to,
                        asset        quantity,
                        std::string  memo );
      };

      using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;
      static constexpr eosio::name active_permission{"active"_n};
};

}