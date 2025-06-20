//
// Created by mwo on 13/04/16.
//

#include "rpccalls.h"

namespace xmreg
{


rpccalls::rpccalls(string _daemon_url,
         uint64_t _timeout)
        : daemon_url {_daemon_url},
          timeout_time {_timeout}
{
    epee::net_utils::parse_url(daemon_url, url);

    port = std::to_string(url.port);

    timeout_time_ms = std::chrono::milliseconds {timeout_time};

    m_http_client.set_server(
            daemon_url,
            boost::optional<epee::net_utils::http::login>{}, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
}

bool
rpccalls::connect_to_arqma_daemon()
{
    //std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

    if(m_http_client.is_connected())
    {
        return true;
    }

    return m_http_client.connect(timeout_time_ms);
}

uint64_t
rpccalls::get_current_height()
{
    COMMAND_RPC_GET_HEIGHT::request   req;
    COMMAND_RPC_GET_HEIGHT::response  res;

    std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

    if(!connect_to_arqma_daemon())
    {
        cerr << "get_current_height: not connected to daemon" << endl;
        return false;
    }

    bool r = epee::net_utils::invoke_http_json(
            "/getheight",
            req, res, m_http_client, timeout_time_ms);

    if (!r)
    {
        cerr << "Error connecting to Arqma daemon at "
             << daemon_url << endl;
        return 0;
    }

    return res.height;
}

bool
rpccalls::get_mempool(vector<tx_info> &mempool_txs)
{

    COMMAND_RPC_GET_TRANSACTION_POOL::request  req;
    COMMAND_RPC_GET_TRANSACTION_POOL::response res;

    bool r;

    {
        std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

        if (!connect_to_arqma_daemon())
        {
            cerr << "get_mempool: not connected to daemon" << endl;
            return false;
        }

        r = epee::net_utils::invoke_http_json(
                "/get_transaction_pool",
                req, res, m_http_client, timeout_time_ms);
    }

    if (!r || res.status != CORE_RPC_STATUS_OK)
    {
        cerr << "Error connecting to Arqma daemon at "
             << daemon_url << endl;
        return false;
    }

    mempool_txs = res.transactions;

    // mempool txs are not sorted base on their arival time,
    // so we sort it here.

    std::sort(mempool_txs.begin(), mempool_txs.end(),
    [](tx_info &t1, tx_info &t2)
    {
        return t1.receive_time > t2.receive_time;
    });

    return true;
}


bool
rpccalls::commit_tx(tools::wallet2::pending_tx &ptx, string &error_msg)
{
    COMMAND_RPC_SEND_RAW_TX::request  req;
    COMMAND_RPC_SEND_RAW_TX::response res;

    req.tx_as_hex = epee::string_tools::buff_to_hex_nodelimer(tx_to_blob(ptx.tx));

    req.do_not_relay = false;

    std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

    if (!connect_to_arqma_daemon())
    {
        cerr << "commit_tx: not connected to daemon" << endl;
        return false;
    }

    bool r = epee::net_utils::invoke_http_json(
            "/sendrawtransaction",
            req, res, m_http_client, timeout_time_ms);

    if (!r || res.status == "Failed")
    {
        error_msg = res.reason;

        cerr << "Error sending tx: " << res.reason << endl;
        return false;
    }

    return true;
}

bool
rpccalls::get_network_info(COMMAND_RPC_GET_INFO::response &response)
{
    epee::json_rpc::request<cryptonote::COMMAND_RPC_GET_INFO::request> req_t{};
    epee::json_rpc::response<cryptonote::COMMAND_RPC_GET_INFO::response, std::string> resp_t{};

    bool r {false};

    req_t.jsonrpc = "2.0";
    req_t.id = epee::serialization::storage_entry(0);
    req_t.method = "get_info";

    {
        std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

        if (!connect_to_arqma_daemon())
        {
            cerr << "get_network_info: not connected to daemon" << endl;
            return false;
        }

        r = epee::net_utils::invoke_http_json("/json_rpc",
                                              req_t, resp_t,
                                              m_http_client);
    }

    string err;

    if (r)
    {
        if (resp_t.result.status == CORE_RPC_STATUS_BUSY)
        {
            err = "daemon is busy. Please try again later.";
        }
        else if (resp_t.result.status != CORE_RPC_STATUS_OK)
        {
            err = resp_t.result.status;
        }

        if (!err.empty())
        {
            cerr << "Error connecting to Arqma daemon due to "
                 << err << endl;
            return false;
        }
    }
    else
    {
        cerr << "Error connecting to Arqma daemon at "
             << daemon_url << endl;
        return false;
    }

    response = resp_t.result;

    return true;
}

bool
rpccalls::get_staking_requirement(uint64_t height, COMMAND_RPC_GET_STAKING_REQUIREMENT::response& response)
{
  epee::json_rpc::request<cryptonote::COMMAND_RPC_GET_STAKING_REQUIREMENT::request> req_t{};
  epee::json_rpc::response<cryptonote::COMMAND_RPC_GET_STAKING_REQUIREMENT::response, std::string> resp_t{};

  bool r {false};

  req_t.params.height = height;
  req_t.jsonrpc = "2.0";
  req_t.id = epee::serialization::storage_entry(0);
  req_t.method = "get_staking_requirement";

  {
    std::lock_guard<std::mutex> quard(m_daemon_rpc_mutex);
    if (!connect_to_arqma_daemon())
    {
      cerr << "get_network_info: not connected to daemon" << endl;
      return false;
    }

    r = epee::net_utils::invoke_http_json("/json_rpc", req_t, resp_t, m_http_client);
  }

  string err;

  if (r)
  {
    if (resp_t.result.status == CORE_RPC_STATUS_BUSY)
    {
      err = "daemon is busy. Please try again later.";
    }
    else if (resp_t.result.status != CORE_RPC_STATUS_OK)
    {
      err = resp_t.result.status;
    }

    if (!err.empty())
    {
      cerr << "Error connecting to Arqma daemon due to " << err << endl;
      return false;
    }
  }
  else
  {
    cerr << "Error connecting to Arqma daemon at " << daemon_url << endl;
    return false;
  }

  response = resp_t.result;
  return true;
}

bool
rpccalls::get_hardfork_info(COMMAND_RPC_HARD_FORK_INFO::response &response)
{
    epee::json_rpc::request<cryptonote::COMMAND_RPC_HARD_FORK_INFO::request> req_t{};
    epee::json_rpc::response<cryptonote::COMMAND_RPC_HARD_FORK_INFO::response, std::string> resp_t{};

    bool r {false};

    req_t.jsonrpc = "2.0";
    req_t.id = epee::serialization::storage_entry(0);
    req_t.method = "hard_fork_info";

    {
        std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

        if (!connect_to_arqma_daemon())
        {
            cerr << "get_hardfork_info: not connected to daemon" << endl;
            return false;
        }

        r = epee::net_utils::invoke_http_json("/json_rpc",
                                              req_t, resp_t,
                                              m_http_client);
    }


    string err;

    if (r)
    {
        if (resp_t.result.status == CORE_RPC_STATUS_BUSY)
        {
            err = "daemon is busy. Please try again later.";
        }
        else if (resp_t.result.status != CORE_RPC_STATUS_OK)
        {
            err = resp_t.result.status;
        }

        if (!err.empty())
        {
            cerr << "Error connecting to Arqma daemon due to "
                 << err << endl;
            return false;
        }
    }
    else
    {
        cerr << "Error connecting to Arqma daemon at "
             << daemon_url << endl;
        return false;
    }

    response = resp_t.result;

    return true;
}



bool
rpccalls::get_dynamic_per_kb_fee_estimate(
        uint64_t grace_blocks,
        uint64_t &fee,
        string &error_msg)
{
    epee::json_rpc::request<COMMAND_RPC_GET_BASE_FEE_ESTIMATE::request> req_t{};
    epee::json_rpc::response<COMMAND_RPC_GET_BASE_FEE_ESTIMATE::response, std::string> resp_t{};

    req_t.jsonrpc = "2.0";
    req_t.id = epee::serialization::storage_entry(0);
    req_t.method = "get_fee_estimate";
    req_t.params.grace_blocks = grace_blocks;

    bool r {false};

    {
        std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

        if (!connect_to_arqma_daemon())
        {
            cerr << "get_dynamic_per_kb_fee_estimate: not connected to daemon" << endl;
            return false;
        }

        r = epee::net_utils::invoke_http_json("/json_rpc",
                                              req_t, resp_t,
                                              m_http_client);
    }

    string err;


    if (r)
    {
        if (resp_t.result.status == CORE_RPC_STATUS_BUSY)
        {
            err = "daemon is busy. Please try again later.";
        }
        else if (resp_t.result.status != CORE_RPC_STATUS_OK)
        {
            err = resp_t.result.status;
        }

        if (!err.empty())
        {
            cerr << "Error connecting to Arqma daemon due to "
                 << err << endl;
            return false;
        }
    }
    else
    {
        cerr << "Error connecting to Arqma daemon at "
             << daemon_url << endl;
        return false;
    }

    fee = resp_t.result.fee;

    return true;
}



bool
rpccalls::get_block(string const &blk_hash, block &blk, string &error_msg)
{
    epee::json_rpc::request<COMMAND_RPC_GET_BLOCK::request> req_t;
    epee::json_rpc::response<COMMAND_RPC_GET_BLOCK::response, std::string> resp_t;

    req_t.jsonrpc = "2.0";
    req_t.id = epee::serialization::storage_entry(0);
    req_t.method = "getblock";
    req_t.params.hash = blk_hash;

    bool r {false};

    {
        std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

        if (!connect_to_arqma_daemon())
        {
            cerr << "get_block: not connected to daemon" << endl;
            return false;
        }

        r = epee::net_utils::invoke_http_json("/json_rpc",
                                              req_t, resp_t,
                                              m_http_client);
    }

    string err;


    if (r)
    {
        if (resp_t.result.status == CORE_RPC_STATUS_BUSY)
        {
            err = "daemon is busy. Please try again later.";
        }
        else if (resp_t.result.status != CORE_RPC_STATUS_OK)
        {
            err = resp_t.result.status;
        }

        if (!err.empty())
        {
            cerr << "Error connecting to Arqma daemon due to "
                 << err << endl;
            return false;
        }
    }
    else
    {
        cerr << "get_staking_requirement: error connecting to Arqma daemon at "
             << daemon_url << endl;
        return false;
    }

    std::string block_bin_blob;

    if(!epee::string_tools::parse_hexstr_to_binbuff(resp_t.result.blob, block_bin_blob))
        return false;

    return parse_and_validate_block_from_blob(block_bin_blob, blk);
}

bool
rpccalls::get_service_node(COMMAND_RPC_GET_SERVICE_NODES::response &res, const std::vector<std::string> &pubkeys)
{
  std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);

  bool result = false;
  if (!connect_to_arqma_daemon())
  {
    cerr << "rpccalls::get_service_node_list_state: not connected to daemon" << endl;
    return result;
  }

  epee::json_rpc::request<COMMAND_RPC_GET_SERVICE_NODES::request> request;
  epee::json_rpc::response<COMMAND_RPC_GET_SERVICE_NODES::response, std::string> response;
  request.params.service_node_pubkeys = pubkeys;
  request.jsonrpc = "2.0";
  request.id = epee::serialization::storage_entry(0);
  request.method = pubkeys.empty() ? "get_all_service_nodes" : "get_service_nodes";

  result = epee::net_utils::invoke_http_json("/json_rpc", request, response, m_http_client, timeout_time_ms);

  if (!result)
    cerr << "Error connecting to Arqma daemon at: " << daemon_url << endl;

  res = response.result;
  return result;
}

bool
rpccalls::get_quorum_state(COMMAND_RPC_GET_QUORUM_STATE::response &res, uint64_t start_height, uint64_t end_height, uint8_t quorum_type)
{
  std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);
  bool result = false;
  if (!connect_to_arqma_daemon())
  {
    cerr << "rpccalls::get_quorum_state:: not connected to daemon" << endl;
    return result;
  }

  epee::json_rpc::request<COMMAND_RPC_GET_QUORUM_STATE::request> request;
  epee::json_rpc::response<COMMAND_RPC_GET_QUORUM_STATE::response, std::string> response;
  request.params.start_height = start_height;
  request.params.end_height = end_height == std::numeric_limits<uint64_t>::max() ? start_height : end_height;
  request.params.quorum_type = quorum_type;
  request.jsonrpc = "2.0";
  request.id = epee::serialization::storage_entry(0);
  request.method = "get_quorum_state";

  result = epee::net_utils::invoke_http_json("/json_rpc", request, response, m_http_client, timeout_time_ms);
  if (!result)
    cerr << "Error connecting to Arqma daemon at: " << daemon_url << endl;

  res = response.result;
  return result;
}

bool rpccalls::get_checkpoints(COMMAND_RPC_GET_CHECKPOINTS::response &res, uint32_t count, uint64_t start_height, uint64_t end_height)
{
  std::lock_guard<std::mutex> guard(m_daemon_rpc_mutex);
  bool result = false;
  if (!connect_to_arqma_daemon())
  {
    cerr << "rpccalls::get_checkpoints: not connected to daemon" << endl;
    return result;
  }

  epee::json_rpc::request<COMMAND_RPC_GET_CHECKPOINTS::request> request;
  epee::json_rpc::response<COMMAND_RPC_GET_CHECKPOINTS::response, std::string> response;
  request.params.count = count;
  request.params.start_height = start_height;
  request.params.end_height = end_height;
  request.jsonrpc = "2.0";
  request.id = epee::serialization::storage_entry(0);
  request.method = "get_checkpoints";

  result = epee::net_utils::invoke_http_json("/json_rpc", request, response, m_http_client, timeout_time_ms);
  if (!result)
    cerr << "Error connecting to Arqma daemon at " << daemon_url << endl;

  res = std::move(response.result);
  return result;
}

}
