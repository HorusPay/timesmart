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
cleos push action horuspay create '{"project":"proj1", "owner":"owner1", "hourly_rate":{"quantity":"5.0000 EOS", "contract":"eosio.token"}}' -p horuspay@active
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
cleos push action horuspay approve '{"project":"proj1", "manager":"manager1", "user":"user1", "seconds":null}' -p manager1@active
```

### read user project table
```shell
cleos get table horuspay horuspay projectuser
```

### user report specific amount of hours
`(units in seconds 30hs = 30*3600 = 108000)`
```shell
cleos push action horuspay addtime '{"project":"proj1", "user":"user1", "seconds":108000, "description":null, "manager":null}' -p user1@active
```

### manager approves all pending hours
```shell
cleos push action horuspay approve '{"project":"proj1", "manager":"manager1", "user":"user1", "seconds":null}' -p manager1@active
```

### manager add specific amount of hours
`(units in seconds 10hs = 10*3600 = 36000)`
```shell
cleos push action horuspay addtime '{"project":"proj1", "user":"user1", "seconds":36000, "description":"fixing", "manager":"manager1"}' -p manager1@active
```

### user report specific amount of hours
`(units in seconds 50hs = 50*3600 = 180000)`
```shell
cleos push action horuspay addtime '{"project":"proj1", "user":"user1", "seconds":180000, "description":null, "manager":null}' -p user1@active
```

### manager decline 40 hours
`(units in seconds 40hs = 40*3600 = 144000)`
```shell
cleos push action horuspay decline '{"project":"proj1", "manager":"manager1", "user":"user1", "seconds":144000}' -p manager1@active
```

### manager approves 20 hours
`(units in seconds 20hs = 20*3600 = 72000)`
```shell
cleos push action horuspay approve '{"project":"proj1", "manager":"manager1", "user":"user1", "seconds":72000}' -p manager1@active
```