project(horuspay)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( horuspay horuspay horuspay.cpp )
target_include_directories( horuspay PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( horuspay ${CMAKE_SOURCE_DIR}/../ricardian )