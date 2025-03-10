#include "optimism.h"

Optimism::Optimism() {
	m_secp256k1 = Ref<Secp256k1Wrapper>(memnew(Secp256k1Wrapper));
	m_keccak = Ref<KeccakWrapper>(memnew(KeccakWrapper));
	m_jsonrpc_helper = Ref<JsonrpcHelper>(memnew(JsonrpcHelper));

	m_req_id = 0;
}

Optimism::~Optimism() {
	;
}

bool Optimism::init_secp256k1_instance() {
	return m_secp256k1->initialize();
}

Ref<Secp256k1Wrapper> Optimism::get_secp256k1_wrapper() {
	return m_secp256k1;
}

Ref<KeccakWrapper> Optimism::get_keccak_wrapper() {
	return m_keccak;
}

Ref<EthAccount> Optimism::get_eth_account() {
    return m_eth_account;
}

void Optimism::set_eth_account(const Ref<EthAccount> &account) {
	m_eth_account = account;
}

String Optimism::get_rpc_url() const {
    return m_rpc_url;
}

// void Optimism::set_rpc_url(const String &url) {
//     m_rpc_url = url;

//     if (m_rpc_url != "" && m_jsonrpc_helper != NULL) {
//         std::regex url_regex(R"(^(https?:\/\/)?([^\/:]+)(:\d+)?(\/.*)?$)");
//         std::smatch url_match_result;

//         std::string rpc_url_str = m_rpc_url.utf8().get_data();

//         if (std::regex_match(rpc_url_str, url_match_result, url_regex)) {
//             std::string protocol = url_match_result[1].str();
//             std::string hostname = url_match_result[2].str();
//             std::string port_str = url_match_result[3].str();

//             int port = 80;
//             if (protocol == "https://") {
//                 port = 443;
//             }

//             if (!port_str.empty()) {
//                 port = std::stoi(port_str.substr(1));
//             }


//             // save protocol prefix
//             std::string full_hostname = protocol + hostname;
//             m_jsonrpc_helper->set_hostname(full_hostname.c_str());
//             m_jsonrpc_helper->set_port(port);
//         } else {
//             ERR_PRINT("Invalid RPC URL format.");
//         }
//     }
// }

void Optimism::set_rpc_url(const String &url) {
    m_rpc_url = url;

    if (m_rpc_url != "" && m_jsonrpc_helper != NULL) {
        std::regex url_regex(R"(^(https?:\/\/[^\/:]+)(:\d+)?(\/.*)?$)");
        std::smatch url_match_result;

        std::string rpc_url_str = m_rpc_url.utf8().get_data();

        if (std::regex_match(rpc_url_str, url_match_result, url_regex)) {
            std::string protocol_and_host = url_match_result[1].str();
            std::string port_str = url_match_result[2].str();
            std::string path_url = url_match_result[3].str();

            int port = 80;
            if (protocol_and_host.find("https://") == 0) {
                port = 443;
            }
            print_line("port_str: " + String(port_str.c_str()));

            if (!port_str.empty()) {
                port = std::stoi(port_str.substr(1));
            }

            std::string full_hostname = protocol_and_host;
            m_jsonrpc_helper->set_hostname(full_hostname.c_str());
            m_jsonrpc_helper->set_port(port);

            if (!path_url.empty()) {
                m_jsonrpc_helper->set_path_url(path_url.c_str());
            } else {
                m_jsonrpc_helper->set_path_url("/");
            }
        } else {
            ERR_PRINT("Invalid RPC URL format: " + m_rpc_url);
        }
    }
}

String Optimism::sign_transaction(const Dictionary &transaction) {
	if (m_eth_account == NULL) {
		ERR_PRINT("Eth account is not set.");
		return "";
	}

	if (transaction.size() == 0) {
		ERR_PRINT("Transaction data is empty.");
		return "";
	}

	Ref<LegacyTx> tx = Ref<LegacyTx>(memnew(LegacyTx));

	// deal with chain id
	if (transaction.has("chainId")) {
		Ref<BigInt> chain_id = Ref<BigInt>(memnew(BigInt));
		chain_id->from_string(transaction["chainId"]);
		tx->set_chain_id(chain_id);
	} else {
		Ref<BigInt> chain_id = this->chain_id();
		tx->set_chain_id(chain_id);
	}

	// deal with nonce
	if (transaction.has("nonce")) {
		tx->set_nonce(transaction["nonce"]);
	} else {
		uint64_t nonce = this->nonce_at(m_eth_account->get_hex_address(), NULL);
		tx->set_nonce(nonce);
	}

	// deal with gas price
	if (transaction.has("gasPrice")) {
		Ref<BigInt> gas_price = Ref<BigInt>(memnew(BigInt));
		gas_price->from_string(transaction["gasPrice"]);
		tx->set_gas_price(gas_price);
	} else {
		Ref<BigInt> gas_price = this->suggest_gas_price();
		tx->set_gas_price(gas_price);
	}

	// deal with to address
	if (transaction.has("to")) {
		tx->set_to_address(transaction["to"]);
	} else {
		tx->set_to_address("");
	}

	// deal with value
	if (transaction.has("value")) {
		Ref<BigInt> value = Ref<BigInt>(memnew(BigInt));
		value->from_string(transaction["value"]);
		tx->set_value(value);
	} else {
		Ref<BigInt> value = Ref<BigInt>(memnew(BigInt));
		value->from_string("0");
		tx->set_value(value);
	}

	// deal with data
	String data_hex_str = String(transaction["data"]);
	if (transaction.has("data")) {
		PackedByteArray data = data_hex_str.hex_decode();
		tx->set_data(data);
	} else {
		tx->set_data(PackedByteArray());
	}

	// deal with gas limit
	if (transaction.has("gasLimit")) {
		tx->set_gas_limit(transaction["gasLimit"]);
	} else {
		Dictionary callmsg = Dictionary();
		callmsg["from"] = m_eth_account->get_hex_address();
		callmsg["to"] = tx->get_to_address();
		callmsg["value"] = tx->get_value()->to_hex();
		callmsg["data"] = "0x" + data_hex_str;

		uint64_t gaslimit = this->estimate_gas(callmsg);
		tx->set_gas_limit(gaslimit); //828516
	}

	int res = tx->sign_tx_by_account(m_eth_account);
	if (res < 0) {
		ERR_PRINT("Failed with sign_tx_by_account.");
		return "";
	}

	return tx->signedtx_marshal_binary();
}


// chain_id() retrieves the current chain ID for transaction replay protection.
Ref<BigInt> Optimism::chain_id(const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Vector<Variant> p_params = Vector<Variant>();
	Dictionary result = m_jsonrpc_helper->call_method("eth_chainId", p_params, req_id);
	if (bool(result["success"]) == false) {
		ERR_PRINT(
			vformat("Failed with calling eth_chainId. errmsg: %s", result["errmsg"])
		);
		return 0;
	}

	if (result["response_body"] == "") {
		ERR_PRINT("eth_chainId response body is empty.");
		return 0;
	}
	Ref<BigInt> chain_id = Ref<BigInt>(memnew(BigInt));
	Ref<JSON> json = Ref<JSON>(memnew(JSON));
	Dictionary res = json->parse_string(result["response_body"]);
	chain_id->from_hex(res["result"]);
	return chain_id;
}

Dictionary Optimism::network_id(const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Vector<Variant> p_params = Vector<Variant>();
	return m_jsonrpc_helper->call_method("net_version", p_params, req_id);
}

// block_by_hash() returns the given full block.
//
// Note that loading full blocks requires two requests. Use header_by_hash()
// if you don't need all transactions or uncle headers.
Dictionary Optimism::block_by_hash(const String &hash, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Vector<Variant> p_params = Vector<Variant>();
	p_params.push_back(hash);
	p_params.push_back(true);
	return m_jsonrpc_helper->call_method("eth_getBlockByHash", p_params, req_id);
}

Dictionary Optimism::async_block_by_hash(const String &hash, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	JSONRPC* jsonrpc = new JSONRPC();

	Vector<String> p_params = Vector<String>();
	p_params.push_back(hash);
	p_params.push_back("true");
	Dictionary request = jsonrpc->make_request("eth_getBlockByHash", p_params, req_id);

	delete jsonrpc;
	return request;
}


// header_by_hash() returns the block header with the given hash.
Dictionary Optimism::header_by_hash(const String &hash, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Vector<Variant> p_params = Vector<Variant>();
	p_params.push_back(hash);
	p_params.push_back(false);
	return m_jsonrpc_helper->call_method("eth_getBlockByHash", p_params, req_id);
}

Dictionary Optimism::async_header_by_hash(const String &hash, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	JSONRPC* jsonrpc = new JSONRPC();

	Vector<String> p_params = Vector<String>();
	p_params.push_back(hash);
	p_params.push_back("false");
	Dictionary request = jsonrpc->make_request("eth_getBlockByHash", p_params, req_id);

	delete jsonrpc;
	return request;
}


// block_by_number() returns a block from the current canonical chain. If number is null, the
// latest known block is returned.
//
// Note that loading full blocks requires two requests. Use header_by_number()
// if you don't need all transactions or uncle headers.
Dictionary Optimism::block_by_number(const Ref<BigInt> &number, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	String number_str = "";
	if ( number == NULL ) {
		number_str = "latest";
	} else {
		if ( number->sgn() < 0 ) {
			Dictionary call_result;
			call_result["success"] = false;
			call_result["errmsg"] = "block number must be positive.";
			return call_result;
		} else {
			number_str = number->to_hex();
		}
	}

	Vector<Variant> p_params = Vector<Variant>();
	p_params.push_back(number_str);
	p_params.push_back(true);
	return m_jsonrpc_helper->call_method("eth_getBlockByNumber", p_params, req_id);
}

// HeaderByNumber returns a block header from the current canonical chain. If number is
// null, the latest known header is returned.
Dictionary Optimism::header_by_number(const Ref<BigInt> &number, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	String number_str = "";
	if ( number == NULL ) {
		number_str = "latest";
	} else {
		if ( number->sgn() < 0 ) {
			Dictionary call_result;
			call_result["success"] = false;
			call_result["errmsg"] = "block number must be positive.";
			return call_result;
		} else {
			number_str = number->to_hex();
		}
	}

	Vector<Variant> p_params = Vector<Variant>();
	p_params.push_back(number_str);
	p_params.push_back(false);
	return m_jsonrpc_helper->call_method("eth_getBlockByNumber", p_params, req_id);
}

// block_number() returns the most recent block number
Dictionary Optimism::block_number(const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Vector<Variant> p_params = Vector<Variant>();
	return m_jsonrpc_helper->call_method("eth_blockNumber", p_params, req_id);
}

Dictionary Optimism::async_block_number(const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	JSONRPC* jsonrpc = new JSONRPC();

	Vector<String> p_params	= Vector<String>();
	Dictionary request = jsonrpc->make_request("eth_blockNumber", p_params, req_id);

	delete jsonrpc;
	return request;
}


String block_number_to_string(int64_t number) {
    switch (number) {
        case 0:
            return "earliest";
        case -1:
            return "pending";
        case -2:
            return "latest";
        case -3:
            return "finalized";
        case -4:
            return "safe";
        default:
            if (number < 0) {
                return String("<invalid ") + String::num_int64(number) + ">";
            }
			return "0x" + String::num_int64(number, 16);
    }
}

// block_receipts_by_number() returns the receipts of a given block number.
// "earliest": 0
// "pending": -1
// "latest": -2
// "finalized": -3
// "safe": -4
// "default": > 0, block number
Dictionary Optimism::block_receipts_by_number(const int64_t &number, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    Vector<Variant> p_params;
    p_params.push_back(block_number_to_string(number));
	return m_jsonrpc_helper->call_method("eth_getBlockReceipts", p_params, req_id);
}

// block_receipts_by_hash() returns the receipts of a given block hash.
Dictionary Optimism::block_receipts_by_hash(const String &hash, const Variant &id) {
	Variant req_id = id;

    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    Vector<Variant> p_params;
    p_params.push_back(hash);
	return m_jsonrpc_helper->call_method("eth_getBlockReceipts", p_params, req_id);
}

// transaction_by_hash() returns the transaction with the given hash.
Dictionary Optimism::transaction_by_hash(const String &hash, const Variant &id) {
	Variant req_id = id;

    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    Vector<Variant> p_params;
    p_params.push_back(hash);
	return m_jsonrpc_helper->call_method("eth_getTransactionByHash", p_params, req_id);
}

// transaction_receipt_by_hash() returns the receipt of a transaction by transaction hash.
// Note that the receipt is not available for pending transactions.
Dictionary Optimism::transaction_receipt_by_hash(const String &hash, const Variant &id) {
	Variant req_id = id;

    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    Vector<Variant> p_params;
    p_params.push_back(hash);
	return m_jsonrpc_helper->call_method("eth_getTransactionReceipt", p_params, req_id);
}

// balance_at() returns the wei balance of the given account.
// The block number can be nil, in which case the balance is taken from the latest known block.
Dictionary Optimism::balance_at(const String &account, const Ref<BigInt> &block_number, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    Vector<Variant> p_params;
    p_params.push_back(account);
    if (block_number != NULL && block_number->sgn() > 0) {
        p_params.push_back(block_number->to_hex());
    } else if (block_number == NULL || block_number->sgn() == 0){
        p_params.push_back("latest");
    } else if (block_number->sgn() < 0) {
        p_params.push_back(block_number_to_string(block_number->to_int64()));
	}

    return m_jsonrpc_helper->call_method("eth_getBalance", p_params, req_id);
}

uint64_t Optimism::nonce_at(const String &account, const Ref<BigInt> &block_number, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    Vector<Variant> p_params;
    p_params.push_back(account);
    if (block_number != NULL && block_number->sgn() > 0) {
        p_params.push_back(block_number->to_hex());
    } else if (block_number == NULL || block_number->sgn() == 0) {
        p_params.push_back("latest");
    } else if (block_number->sgn() < 0) {
        p_params.push_back(block_number_to_string(block_number->to_int64()));
    }

    Dictionary result = m_jsonrpc_helper->call_method("eth_getTransactionCount", p_params, req_id);
	if (bool(result["success"]) == false) {
		ERR_PRINT(
			vformat("Failed with calling eth_getTransactionCount. errmsg: %s", result["errmsg"])
		);
		return 0;
	}
	if (result["response_body"] == "") {
		ERR_PRINT("eth_getTransactionCount response body is empty.");
		return 0;
	}
	int64_t nonce = 0;
	Ref<JSON> json = Ref<JSON>(memnew(JSON));
	print_line("nonce_at result: " + String(result["response_body"]));
	Dictionary res = json->parse_string(result["response_body"]);
	nonce = String(res["result"]).hex_to_int();
	return uint64_t(nonce);
}

// send_transaction injects a signed transaction into the pending pool for execution.
//
// If the transaction was a contract creation use the TransactionReceipt method to get the
// contract address after the transaction has been mined.
//
// signed_tx: The signed transaction data encode as a hex string with 0x prefix.
Dictionary Optimism::send_transaction(const String &signed_tx, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Dictionary ret = Dictionary();
	ret["success"] = true;
	ret["errmsg"] = "";

	Vector<Variant> p_params = Vector<Variant>();
	p_params.push_back(signed_tx);
	Dictionary result = m_jsonrpc_helper->call_method("eth_sendRawTransaction", p_params, req_id);
	if (bool(result["success"]) == false) {
		ERR_PRINT(
			vformat("Failed with calling eth_sendRawTransaction. errmsg: %s", result["errmsg"])
		);
		ret["success"] = false;
		ret["errmsg"] = result["errmsg"];
		return ret;
	}
	if (result["response_body"] == "") {
		ERR_PRINT("eth_sendRawTransaction response body is empty.");
		ret["success"] = false;
		ret["errmsg"] = "response body is empty.";
		return ret;
	}
	print_line("eth_sendRawTransaction result: " + String(result["response_body"]));
	String tx_hash = "";
	Ref<JSON> json = Ref<JSON>(memnew(JSON));
	Dictionary res = json->parse_string(result["response_body"]);
	tx_hash = String(res["result"]);
	ret["txhash"] = tx_hash;
	return ret;
}

Dictionary Optimism::async_send_transaction(const String &signed_tx, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (req_id == "") {
		req_id = String::num_int64(m_req_id);
	}

	JSONRPC *jsonrpc = new JSONRPC();

	Vector<String> p_params	= Vector<String>();
	p_params.push_back(signed_tx);
	Dictionary request = jsonrpc->make_request("eth_sendRawTransaction", p_params, req_id);
	delete jsonrpc;
	return request;
}

// call_contract executes a message call transaction, which is directly executed in the VM
// of the node, but never mined into the blockchain.
//
// blockNumber selects the block height at which the call runs. It can be nil, in which
// case the code is taken from the latest known block. Note that state from very old
// blocks might not be available.
Dictionary Optimism::call_contract(Dictionary call_msg, const String &block_number, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Vector<Variant> p_params = Vector<Variant>();

	if ( !call_msg.has("from") || call_msg["from"] == "" ) {
		call_msg["from"] = m_eth_account->get_hex_address();
	}

	if ( call_msg.has("input") && !has_hex_prefix(String(call_msg["input"])) ) {
		call_msg["input"] = "0x" + String(call_msg["input"]);
	}

	// first param: call msg
	p_params.push_back(call_msg);
	// second param: block number
	if ( block_number == "" ) {
		p_params.push_back("latest");
	} else {
		p_params.push_back(block_number);
	}
	return m_jsonrpc_helper->call_method("eth_call", p_params, req_id);
}

// suggest_gas_price retrieves the currently suggested gas price to allow a timely
// execution of a transaction.
Ref<BigInt> Optimism::suggest_gas_price(const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Vector<Variant> p_params = Vector<Variant>();
	Dictionary result =  m_jsonrpc_helper->call_method("eth_gasPrice", p_params, req_id);
	if (bool(result["success"]) == false) {
		ERR_PRINT(
			vformat("Failed with calling eth_gasPrice. errmsg: %s", result["errmsg"])
		);
		return NULL;
	}

	if (result["response_body"] == "") {
		ERR_PRINT("eth_gasPrice response body is empty.");
		return NULL;
	}

	Ref<BigInt> gas_price = Ref<BigInt>(memnew(BigInt));
	Ref<JSON> json = Ref<JSON>(memnew(JSON));
	Dictionary res = json->parse_string(result["response_body"]);
	gas_price->from_hex(res["result"]);
	return gas_price;
}



// EstimateGas tries to estimate the gas needed to execute a specific transaction based on
// the current pending state of the backend blockchain. There is no guarantee that this is
// the true gas limit requirement as other transactions may be added or removed by miners,
// but it should provide a basis for setting a reasonable default.
uint64_t Optimism::estimate_gas(const Dictionary &call_msg, const Variant &id) {
	Variant req_id = id;
	m_req_id++;
	if (id == "") {
		req_id = String::num_int64(m_req_id);
	}

	Vector<Variant> p_params = Vector<Variant>();
	p_params.push_back(call_msg);
	Dictionary result = m_jsonrpc_helper->call_method("eth_estimateGas", p_params, req_id);
	if (bool(result["success"]) == false) {
		ERR_PRINT(
			vformat("Failed with calling eth_estimateGas. errmsg: %s", result["errmsg"])
		);
		return 0;
	}
	if (result["response_body"] == "") {
		ERR_PRINT("eth_estimateGas response body is empty.");
		return 0;
	}
	int64_t gas_limit = 0;
	Ref<JSON> json = Ref<JSON>(memnew(JSON));
	print_line("estimate_gas result: " + String(result["response_body"]));
	Dictionary res = json->parse_string(result["response_body"]);
	gas_limit = String(res["result"]).hex_to_int();
	return uint64_t(gas_limit);
}

Dictionary Optimism::async_block_by_number(const Ref<BigInt> &number, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    String number_str = "";
    if (number == NULL) {
        number_str = "latest";
    } else {
        if (number->sgn() < 0) {
            Dictionary request;
            request["success"] = false;
            request["errmsg"] = "block number must be positive.";
            delete jsonrpc;
            return request;
        } else {
            number_str = number->to_hex();
        }
    }

    Vector<String> p_params = Vector<String>();
    p_params.push_back(number_str);
    p_params.push_back("true");
    Dictionary request = jsonrpc->make_request("eth_getBlockByNumber", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_block_receipts_by_number(const int64_t &number, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    Vector<String> p_params = Vector<String>();
    p_params.push_back(block_number_to_string(number));
    Dictionary request = jsonrpc->make_request("eth_getBlockReceipts", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_block_receipts_by_hash(const String &hash, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    Vector<String> p_params = Vector<String>();
    p_params.push_back(hash);
    Dictionary request = jsonrpc->make_request("eth_getBlockReceipts", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_transaction_by_hash(const String &hash, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    Vector<String> p_params = Vector<String>();
    p_params.push_back(hash);
    Dictionary request = jsonrpc->make_request("eth_getTransactionByHash", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_transaction_receipt_by_hash(const String &hash, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    Vector<String> p_params = Vector<String>();
    p_params.push_back(hash);
    Dictionary request = jsonrpc->make_request("eth_getTransactionReceipt", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_balance_at(const String &account, const Ref<BigInt> &block_number, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    Vector<String> p_params = Vector<String>();
    p_params.push_back(account);
    if (block_number != NULL && block_number->sgn() > 0) {
        p_params.push_back(block_number->to_hex());
    } else if (block_number == NULL || block_number->sgn() == 0) {
        p_params.push_back("latest");
    } else if (block_number->sgn() < 0) {
        p_params.push_back(block_number_to_string(block_number->to_int64()));
    }

    Dictionary request = jsonrpc->make_request("eth_getBalance", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_nonce_at(const String &account, const Ref<BigInt> &block_number, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    Vector<String> p_params = Vector<String>();
    p_params.push_back(account);
    if (block_number != NULL && block_number->sgn() > 0) {
        p_params.push_back(block_number->to_hex());
    } else if (block_number == NULL || block_number->sgn() == 0) {
        p_params.push_back("latest");
    } else if (block_number->sgn() < 0) {
        p_params.push_back(block_number_to_string(block_number->to_int64()));
    }

    Dictionary request = jsonrpc->make_request("eth_getTransactionCount", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_call_contract(Dictionary call_msg, const String &block_number, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    if (!call_msg.has("from") || call_msg["from"] == "") {
        call_msg["from"] = m_eth_account->get_hex_address();
    }

    if (call_msg.has("input") && !has_hex_prefix(String(call_msg["input"]))) {
        call_msg["input"] = "0x" + String(call_msg["input"]);
    }

    Vector<Variant> p_params = Vector<Variant>();
    p_params.push_back(call_msg);
    p_params.push_back(block_number == "" ? "latest" : block_number);

    Dictionary request = jsonrpc->make_request("eth_call", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_suggest_gas_price(const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    Vector<String> p_params = Vector<String>();
    Dictionary request = jsonrpc->make_request("eth_gasPrice", p_params, req_id);

    delete jsonrpc;
    return request;
}

Dictionary Optimism::async_estimate_gas(const Dictionary &call_msg, const Variant &id) {
    Variant req_id = id;
    m_req_id++;
    if (id == "") {
        req_id = String::num_int64(m_req_id);
    }

    JSONRPC* jsonrpc = new JSONRPC();

    Vector<Variant> p_params = Vector<Variant>();
    p_params.push_back(call_msg);
    Dictionary request = jsonrpc->make_request("eth_estimateGas", p_params, req_id);

    delete jsonrpc;
    return request;
}

void Optimism::_bind_methods() {
	ClassDB::bind_method(D_METHOD("init_secp256k1_instance"), &Optimism::init_secp256k1_instance);
	ClassDB::bind_method(D_METHOD("get_secp256k1_wrapper"), &Optimism::get_secp256k1_wrapper);
	ClassDB::bind_method(D_METHOD("get_keccak_wrapper"), &Optimism::get_keccak_wrapper);
    ClassDB::bind_method(D_METHOD("get_rpc_url"), &Optimism::get_rpc_url);
    ClassDB::bind_method(D_METHOD("set_rpc_url", "url"), &Optimism::set_rpc_url);
	ClassDB::bind_method(D_METHOD("get_eth_account"), &Optimism::get_eth_account);
	ClassDB::bind_method(D_METHOD("set_eth_account", "account"), &Optimism::set_eth_account);

	ClassDB::bind_method(D_METHOD("sign_transaction", "transaction"), &Optimism::sign_transaction);

    // sync jsonrpc method
    ClassDB::bind_method(D_METHOD("chain_id", "id"), &Optimism::chain_id, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("block_by_hash", "hash", "id"), &Optimism::block_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("header_by_hash", "hash", "id"), &Optimism::header_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("block_by_number", "number", "id"), &Optimism::block_by_number, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("header_by_number", "number", "id"), &Optimism::header_by_number, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("block_number", "id"), &Optimism::block_number, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("block_receipts_by_number", "number", "id"), &Optimism::block_receipts_by_number, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("block_receipts_by_hash", "hash", "id"), &Optimism::block_receipts_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("transaction_by_hash", "hash", "id"), &Optimism::transaction_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("transaction_receipt_by_hash", "hash", "id"), &Optimism::transaction_receipt_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("balance_at", "account", "block_number", "id"), &Optimism::balance_at, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("nonce_at", "account", "block_number", "id"), &Optimism::nonce_at, DEFVAL(Ref<BigInt>()), DEFVAL(Variant()));

    ClassDB::bind_method(D_METHOD("send_transaction", "signed_tx", "id"), &Optimism::send_transaction, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("call_contract", "call_msg", "block_number", "id"), &Optimism::call_contract, DEFVAL(""), DEFVAL(""));
    ClassDB::bind_method(D_METHOD("suggest_gas_price", "id"), &Optimism::suggest_gas_price, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("estimate_gas", "call_msg", "id"), &Optimism::estimate_gas, DEFVAL(""));

    // async jsonrpc method
    ClassDB::bind_method(D_METHOD("async_send_transaction", "signed_tx", "id"), &Optimism::async_send_transaction, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_block_by_hash", "hash", "id"), &Optimism::async_block_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_block_by_number", "number", "id"), &Optimism::async_block_by_number, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_block_number", "id"), &Optimism::async_block_number, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_block_receipts_by_number", "number", "id"), &Optimism::async_block_receipts_by_number, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_block_receipts_by_hash", "hash", "id"), &Optimism::async_block_receipts_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_transaction_by_hash", "hash", "id"), &Optimism::async_transaction_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_transaction_receipt_by_hash", "hash", "id"), &Optimism::async_transaction_receipt_by_hash, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_balance_at", "account", "block_number", "id"), &Optimism::async_balance_at, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_nonce_at", "account", "block_number", "id"), &Optimism::async_nonce_at, DEFVAL(Ref<BigInt>()), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("async_header_by_hash", "hash", "id"), &Optimism::async_header_by_hash, DEFVAL(""));
	//ClassDB::bind_method(D_METHOD("async_header_by_number", "number", "id"), &Optimism::async_header_by_number, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_call_contract", "call_msg", "block_number", "id"), &Optimism::async_call_contract, DEFVAL(""), DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_suggest_gas_price", "id"), &Optimism::async_suggest_gas_price, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("async_estimate_gas", "call_msg", "id"), &Optimism::async_estimate_gas, DEFVAL(""));
}

