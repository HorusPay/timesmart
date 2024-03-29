#include <horuspay.hpp>
#include <eosio/system.hpp>

namespace horuspay {

void horuspay::create(name project, name owner, extended_asset hourly_rate) {

   require_auth(_self);

   eosio::check(hourly_rate.quantity.amount > 0, "Hourly rate must be positive");
   eosio::check(hourly_rate.quantity.symbol.is_valid(), "Invalid hourly rate symbol");
   eosio::check(eosio::is_account(hourly_rate.contract), "Invalid token account");

   project_table _projects(_self, _self.value);

   auto prj = _projects.find(project.value);
   eosio::check(prj == _projects.end(), "A project with that name already exists");

   _projects.emplace(_self, [&](auto& p){
      p.name        = project;
      p.hourly_rate = hourly_rate;
      p.balance     = hourly_rate;
      p.balance.quantity.amount = 0;
   });

   project_manager_table _project_managers(_self, _self.value);
   _project_managers.emplace(_self, [&](auto& pa){
      pa.id       = _project_managers.available_primary_key();
      pa.manager  = owner;
      pa.project  = project;
      pa.is_owner = true;
   });

}

void horuspay::on_transfer( name from, name to, asset quantity, const std::string& memo ) {

   if(from == _self) return;

   name project = name(memo);
   // print("on_transfer: [", memo, "][", project, "]");

   project_table _projects(_self, _self.value);
   auto prj = _projects.find(project.value);
   eosio::check(prj != _projects.end(), "transfer project not found");

   project_manager_table _project_managers(_self, _self.value);
   auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
   auto& pa = projmanager_inx.get(compute_key(from.value, project.value), "only project managers can deposit");
   eosio::check(pa.project == project, "internal error");

   //Valiate received token symbol with the one configured for the project
   eosio::check(quantity.symbol == prj->balance.quantity.symbol, "invalid deposit token");
   eosio::check(get_first_receiver() == prj->balance.contract, "invalid deposit contract");

   //Update project total balance
   _projects.modify(prj, same_payer, [&](auto& p){
      p.balance.quantity += quantity;
   });
}

void horuspay::adduser(name project, name manager, name user) {

   require_auth(manager);

   project_table _projects(_self, _self.value);
   const auto& prj = _projects.get(project.value, "project not found");

   project_manager_table _project_managers(_self, _self.value);
   auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
   auto& pa = projmanager_inx.get(compute_key(manager.value, project.value), "only project manager can add users");

   project_user_table _project_users(_self, _self.value);
   auto projuser_inx = _project_users.get_index<"byusr"_n>();
   auto pu = projuser_inx.find(compute_key(user.value, project.value));
   eosio::check(pu == projuser_inx.end(), "the user is already a member of the project");
   eosio::check(eosio::is_account(user), "user must be a registered account");

   _project_users.emplace(_self, [&](auto& u){
      u.id          = _project_users.available_primary_key();
      u.project     = project;
      u.user        = user;
      u.pending     = 0;
      u.hourly_rate = prj.hourly_rate;
      u.last_clock  = decltype(u.last_clock)(0);
   });
}

void horuspay::removeuser(name project, name manager, name user) {
   
   require_auth(manager);
   
   project_manager_table _project_managers(_self, _self.value);
   auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
   auto& pa = projmanager_inx.get(compute_key(manager.value, project.value), "only project admins can remove users");

   project_user_table _project_users(_self, _self.value);
   auto projuser_inx = _project_users.get_index<"byusr"_n>();
   auto pu = projuser_inx.find(compute_key(user.value, project.value));
   eosio::check(pu != projuser_inx.end(), "the user is not member of the project");
   eosio::check(pu->pending == 0, "the user has pending hours");

   projuser_inx.erase(pu);
}

void horuspay::addmanager(name project, name owner, name manager) {

   require_auth(owner);
   
   project_manager_table _project_managers(_self, _self.value);
   auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
   auto pa = projmanager_inx.find(compute_key(owner.value, project.value));
   eosio::check(pa != projmanager_inx.end() && pa->is_owner == true, "only project owner can add new managers");

   auto npa = projmanager_inx.find(compute_key(manager.value, project.value));
   eosio::check(npa == projmanager_inx.end(), "manager is already a manager of the project");
   eosio::check(eosio::is_account(manager), "manager must be a registered account");

   _project_managers.emplace(_self, [&](auto& m){
      m.id        = _project_managers.available_primary_key();
      m.project   = project;
      m.manager   = manager;
      m.is_owner  = false;
   });
}

void horuspay::rmvmanager(name project, name owner, name manager) {
   
   require_auth(owner);
   
   project_manager_table _project_managers(_self, _self.value);
   auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
   auto pa = projmanager_inx.find(compute_key(owner.value, project.value));
   eosio::check(pa != projmanager_inx.end() && pa->is_owner == true, "only project owner can remove managers");

   auto mgr = projmanager_inx.find(compute_key(manager.value, project.value));
   eosio::check(mgr != projmanager_inx.end(), "not a manager of the project");

   projmanager_inx.erase(mgr);
}

void horuspay::clockin(name project, name user) {

   require_auth(user);

   project_user_table _project_users(_self, _self.value);
   auto projuser_inx = _project_users.get_index<"byusr"_n>();
   auto pu_itr = projuser_inx.find(compute_key(user.value, project.value));
   eosio::check(pu_itr != projuser_inx.end(), "the user is not a member of the project");

   _project_users.modify(*pu_itr, same_payer, [&](auto& pu){
      pu.last_clock = eosio::current_block_time();
   });
}

void horuspay::clockout(name project, name user, optional<string> description) {
   
   require_auth(user);

   project_user_table _project_users(_self, _self.value);
   auto projuser_inx = _project_users.get_index<"byusr"_n>();
   auto pu_itr = projuser_inx.find(compute_key(user.value, project.value));
   eosio::check(pu_itr != projuser_inx.end(), "the user is not a member of the project");
   eosio::check(pu_itr->last_clock.slot != 0, "must clockin first");

   auto total = eosio::time_point(eosio::current_block_time().to_time_point() - pu_itr->last_clock.to_time_point()).sec_since_epoch();
   eosio::check(total > 0, "time too small to account");

   _project_users.modify(*pu_itr, same_payer, [&](auto& pu){
      pu.pending        += total;
      pu.last_clock.slot = 0;
   });
}

void horuspay::addtime(name project, name user, uint64_t seconds, optional<string> description, optional<name> manager) {
   
   eosio::check(seconds > 0, "seconds must be positive");

   if(manager) {
      require_auth(*manager);
      project_manager_table _project_managers(_self, _self.value);
      auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
      projmanager_inx.get(compute_key(manager->value, project.value), "not a manager of the project");
   } else {
      require_auth(user);
   }

   project_user_table _project_users(_self, _self.value);
   auto projuser_inx = _project_users.get_index<"byusr"_n>();
   auto pu_itr = projuser_inx.find(compute_key(user.value, project.value));
   eosio::check(pu_itr != projuser_inx.end(), "the user is not a member of the project");

   _project_users.modify(*pu_itr, same_payer, [&](auto& pu){
      pu.pending += seconds;
   });
}

void horuspay::approve(name project, name manager, name user, optional<int64_t> seconds) {

   require_auth(manager);

   project_table _projects(_self, _self.value);
   const auto& prj = _projects.get(project.value, "project not found");

   project_manager_table _project_managers(_self, _self.value);
   auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
   projmanager_inx.get(compute_key(manager.value, project.value), "only managers can approve hours");

   project_user_table _project_users(_self, _self.value);
   auto projuser_inx = _project_users.get_index<"byusr"_n>();
   auto pu = projuser_inx.find(compute_key(user.value, project.value));
   eosio::check(pu != projuser_inx.end(), "the user is not a member of the project");

   int64_t secs_to_approve = pu->pending;
   if(seconds) {
      eosio::check(seconds > 0 && seconds <= secs_to_approve, "0 < approve <= pending");
      secs_to_approve = *seconds;
   }

   projuser_inx.modify(pu, same_payer, [&](auto& p){
      p.pending -= secs_to_approve;
   });

   double total_hours = double(secs_to_approve)/double(3600);

   auto q = pu->hourly_rate.quantity;
   auto payment = asset(int64_t(double(q.amount)*total_hours), q.symbol);
   eosio::check(prj.balance.quantity >= payment, "not enough funds");

   {
      std::string memo("horuspay");
      transfer_action transfer_act{ prj.balance.contract, { _self, active_permission } };
      transfer_act.send( _self, user, payment, memo );
   }

   _projects.modify(prj, same_payer, [&](auto& p) {
      p.balance.quantity -= payment;
   });

}

void horuspay::decline(name project, name manager, name user, int64_t seconds) {
   
   require_auth(manager);

   project_manager_table _project_managers(_self, _self.value);
   auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
   projmanager_inx.get(compute_key(manager.value, project.value), "only managers can decline hours");

   project_user_table _project_users(_self, _self.value);
   auto projuser_inx = _project_users.get_index<"byusr"_n>();
   auto pu = projuser_inx.find(compute_key(user.value, project.value));
   eosio::check(pu != projuser_inx.end(), "the user is not a member of the project");

   eosio::check(seconds > 0 && seconds <= pu->pending, "0 < decline <= pending");
   
   projuser_inx.modify(pu, same_payer, [&](auto& p){
      p.pending  -= seconds;
   });
}

void horuspay::setuserrate(name project, name manager, name user, extended_asset hourly_rate) {

   require_auth(manager);
   project_table _projects(_self, _self.value);
   const auto& prj = _projects.get(project.value, "project not found");

   eosio::check(prj.hourly_rate.contract == hourly_rate.contract &&
      prj.hourly_rate.quantity.symbol == hourly_rate.quantity.symbol, "hourly rate asset/contract should be the same as project");
      
   project_manager_table _project_managers(_self, _self.value);
   auto projmanager_inx = _project_managers.get_index<"bymgr"_n>();
   projmanager_inx.get(compute_key(manager.value, project.value), "only managers can change user hourly rate");

   project_user_table _project_users(_self, _self.value);
   auto projuser_inx = _project_users.get_index<"byusr"_n>();
   auto pu = projuser_inx.find(compute_key(user.value, project.value));
   eosio::check(pu != projuser_inx.end(), "the user is not a member of the project");

   projuser_inx.modify(pu, same_payer, [&](auto& p){
      p.hourly_rate = hourly_rate;
   });
}

}
