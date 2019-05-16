## Pre-requisites
- EOS 1.8-rc2 (or newwer)
https://github.com/EOSIO/eos/releases


- CDT v1.6.1 (or newwer)
https://github.com/EOSIO/eosio.cdt/releases

- EOS Contracts v1.6.0
https://github.com/EOSIO/eosio.contracts/releases

## Build contract

```shell
mkdir -p build && cd build
cmake ..
make
```

## Build tests (optional)
- #### *eosio.contracts* must be built

```shell
cd tests
mkdir -p build && cd build
cmake -DEOS_CONTRACTS_BUILD_FOLDER=/path/to/eosio.contracts/build/contracts ..
make
./unit_test
```


## Setup horuspay contract

- inside the *horuspay/build* folder

```shell
cleos create account eosio horuspay EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos set account permission --add-code horuspay active -p horuspay@active

cleos set contract horuspay horuspay -p horuspay@active
```

## Setup token contract

```shell
cleos create account eosio eosio.token EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos set contract eosio.token /path/to/eosio.contracts/build/contracts/eosio.token

cleos push action eosio.token create '[ "eosio", "10000000000.0000 EOS"]' -p eosio.token
cleos push action eosio.token issue '[ "eosio", "1000000000.0000 EOS", "all supply"]' -p eosio

```

## Basic interaction

### Setup test accounts

```shell
cleos create account eosio user1 EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos create account eosio user2 EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos create account eosio owner1 EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos create account eosio manager1 EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV

cleos transfer eosio owner1 "10000.0000 EOS" -p eosio
cleos transfer eosio manager1 "10000.0000 EOS" -p eosio
```

### create a new project

```
cleos push action horuspay create '{"project":"proj1", "owner":"owner1", "hourly_rate":{"quantity":"5.0000 EOS", "contract":"eosio.token"}}' -p owner1@active
```

### add user to project
```shell
cleos push action horuspay adduser '["proj1", "owner1", "user1"]' -p owner1@active
```

### add manager to project
```shell
cleos push action horuspay addmanager '["proj1", "owner1", "manager1"]' -p owner1@active
```

### fund project `proj1` with `1000 EOS`
```shell
cleos transfer owner1 horuspay "1000.0000 EOS" "proj1" -p owner1@active
```

### user clock-in
```shell
cleos push action horuspay clockin '["proj1", "user1"]' -p user1@active
```

### read user project table
```shell
cleos get table horuspay horuspay projectuser
```

### (after some time) user clock-out
```shell
cleos push action horuspay clockout '["proj1", "user1", ""]' -p user1@active
```

### manager approves all pending hours
```shell
cleos push action horuspay approve '{"project":"proj1", "manager":"manager1", "user":"user1", "hours":null}' -p manager1@active
```

### read user project table
```shell
cleos get table horuspay horuspay projectuser
```

### user claim payment
```shell
cleos push action horuspay claim '{"project":"proj1", "user":"user1", "hours":null}' -p user1@active
```

### user report specific amount of hours
```shell
cleos push action horuspay addtime '["proj1", "user1", 30, ""]' -p user1@active
```

### manager approves all pending hours
```shell
cleos push action horuspay approve '{"project":"proj1", "manager":"manager1", "user":"user1", "hours":null}' -p manager1@active
```

### user claim payment
```shell
cleos push action horuspay claim '{"project":"proj1", "user":"user1", "hours":null}' -p user1@active
```

### user report specific amount of hours
```shell
cleos push action horuspay addtime '["proj1", "user1", 50, ""]' -p user1@active
```

### manager approves 10 hours
```shell
cleos push action horuspay approve '{"project":"proj1", "manager":"manager1", "user":"user1", "hours":10}' -p manager1@active
```

### user claim payment
```shell
cleos push action horuspay claim '{"project":"proj1", "user":"user1", "hours":null}' -p user1@active
```
