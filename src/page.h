//
// Created by mwo on 8/04/16.
//

#ifndef CROWXMR_PAGE_H
#define CROWXMR_PAGE_H

#include "mstch/mstch.hpp"

#include "arqma_headers.h"
#include "randomx.h"
#include "common.hpp"
#include "blake2/blake2.h"
#include "virtual_machine.hpp"
#include "program.hpp"
#include "aes_hash.hpp"
#include "assembly_generator_x86.hpp"

#include "../gen/version.h"

#include "MicroCore.h"
#include "tools.h"
#include "rpccalls.h"

#include "CurrentBlockchainStatus.h"
#include "MempoolStatus.h"

#include "../ext/crow/crow.h"

#include "../ext/json.hpp"
#include "../ext/mstch/src/visitor/render_node.hpp"


#include <algorithm>
#include <limits>
#include <ctime>
#include <future>
#include <type_traits>

#define TMPL_DIR                    "./templates"
#define TMPL_PARTIALS_DIR           TMPL_DIR "/partials"
#define TMPL_CSS_STYLES             TMPL_DIR "/css/style.css"
#define TMPL_INDEX2                 TMPL_DIR "/index2.html"
#define TMPL_MEMPOOL                TMPL_DIR "/mempool.html"
#define TMPL_ALTBLOCKS              TMPL_DIR "/altblocks.html"
#define TMPL_MEMPOOL_ERROR          TMPL_DIR "/mempool_error.html"
#define TMPL_HEADER                 TMPL_DIR "/header.html"
#define TMPL_FOOTER                 TMPL_DIR "/footer.html"
#define TMPL_BLOCK                  TMPL_DIR "/block.html"
#define TMPL_TX                     TMPL_DIR "/tx.html"
#define TMPL_ADDRESS                TMPL_DIR "/address.html"
#define TMPL_MY_OUTPUTS             TMPL_DIR "/my_outputs.html"
#define TMPL_SEARCH_RESULTS         TMPL_DIR "/search_results.html"
#define TMPL_MY_RAWTX               TMPL_DIR "/rawtx.html"
#define TMPL_MY_CHECKRAWTX          TMPL_DIR "/checkrawtx.html"
#define TMPL_MY_PUSHRAWTX           TMPL_DIR "/pushrawtx.html"
#define TMPL_MY_RAWKEYIMGS          TMPL_DIR "/rawkeyimgs.html"
#define TMPL_MY_CHECKRAWKEYIMGS     TMPL_DIR "/checkrawkeyimgs.html"
#define TMPL_MY_RAWOUTPUTKEYS       TMPL_DIR "/rawoutputkeys.html"
#define TMPL_MY_CHECKRAWOUTPUTKEYS  TMPL_DIR "/checkrawoutputkeys.html"
#define TMPL_SERVICE_NODES          TMPL_DIR "/service_nodes.html"
#define TMPL_SERVICE_NODES_DETAIL   TMPL_DIR "/service_node_detail.html"
#define TMPL_QUORUM_STATES          TMPL_DIR "/quorum_states.html"
#define TMPL_CHECKPOINTS            TMPL_DIR "/checkpoints.html"

#define TMPL_MANIFEST               TMPL_DIR "/site.manifest"

#define ARQMAEXPLORER_RPC_VERSION_MAJOR 7
#define ARQMAEXPLORER_RPC_VERSION_MINOR 0
#define MAKE_ARQMAEXPLORER_RPC_VERSION(major,minor) (((major)<<16)|(minor))
#define ARQMAEXPLORER_RPC_VERSION MAKE_ARQMAEXPLORER_RPC_VERSION(ARQMAEXPLORER_RPC_VERSION_MAJOR, ARQMAEXPLORER_RPC_VERSION_MINOR)

/**
 * visitor to produce json representations of
 * values stored in mstch::node
 */
class mstch_node_to_json : public boost::static_visitor<nlohmann::json>
{
public:

    // enabled for numeric types
    template<typename T>
    typename std::enable_if<std::is_arithmetic<T>::value, nlohmann::json>::type
    operator()(T const& value) const {
        return nlohmann::json {value};
    }

    nlohmann::json operator()(std::string const& value) const {
        return nlohmann::json {value};
    }

    nlohmann::json operator()(mstch::map const& n_map) const
    {
        nlohmann::json j;

        for (auto const& kv: n_map)
            j[kv.first] = boost::apply_visitor(mstch_node_to_json(), kv.second);

        return j;
    }

    nlohmann::json operator()(mstch::array const& n_array) const
    {
        nlohmann::json j;

        for (auto const& v:  n_array)
            j.push_back(boost::apply_visitor(mstch_node_to_json(), v));

        return j;

    }

    // catch other types that are non-numeric and not listed above
    template<typename T>
    typename std::enable_if<!std::is_arithmetic<T>::value, nlohmann::json>::type
    operator()(const T&) const {
        return nlohmann::json {};
    }

};

namespace mstch
{
namespace internal
{
    // add conversion from mstch::map to nlohmann::json
    void
    to_json(nlohmann::json& j, mstch::map const &m)
    {
        for (auto const &kv: m)
            j[kv.first] = boost::apply_visitor(mstch_node_to_json(), kv.second);
    }
}
}

namespace xmreg
{

using namespace cryptonote;
using namespace crypto;
using namespace std;

using epee::string_tools::pod_to_hex;
using epee::string_tools::hex_to_pod;

template< typename T >
std::string as_hex(T i)
{
  std::stringstream ss;

  ss << "0x" << setfill ('0') << setw(sizeof(T)*2)
         << hex << i;
  return ss.str();
}

/**
* @brief The tx_details struct
*
* Basic information about tx
*
*/
struct tx_details
{
    crypto::hash hash;
    crypto::hash prefix_hash;
    crypto::public_key pk;
    std::vector<crypto::public_key> additional_pks;
    uint64_t arq_inputs;
    uint64_t arq_outputs;
    uint64_t num_nonrct_inputs;
    uint64_t fee;
    uint64_t mixin_no;
    uint64_t size;
    uint64_t blk_height;
    size_t   version;

    bool has_additional_tx_pub_keys {false};

    char     pID; // '-' - no payment ID,
                  // 'l' - legacy, long 64 character payment id,
                  // 'e' - encrypted, short, from integrated addresses
                  // 's' - sub-address (avaliable only for multi-output txs)
    uint64_t unlock_time;
    uint64_t no_confirmations;
    vector<uint8_t> extra;

    crypto::hash  payment_id  = null_hash; // normal
    crypto::hash8 payment_id8 = null_hash8; // encrypted

    string payment_id_as_ascii;

    std::vector<std::vector<crypto::signature>> signatures;

    // key images of inputs
    vector<txin_to_key> input_key_imgs;

    // public keys and arq amount of outputs
    vector<pair<txout_to_key, uint64_t>> output_pub_keys;

    mstch::map
    get_mstch_map() const
    {
        string mixin_str {"N/A"};
        string fee_str {"N/A"};
        string fee_short_str {"N/A"};
        string payed_for_kB_str {""};
        string fee_nano_str {"N/A"};
        string payed_for_kB_nano_str {"N/A"};

        const double& arq_amount = ARQ_AMOUNT(fee);

        // tx size in kB
        double tx_size =  static_cast<double>(size)/1024.0;

        if (!input_key_imgs.empty())
        {
            double payed_for_kB = arq_amount / tx_size;

            mixin_str             = std::to_string(mixin_no);
            fee_str               = fmt::format("{:0.9f}", arq_amount);
            fee_short_str         = fmt::format("{:0.9f}", arq_amount);
            fee_nano_str          = fmt::format("{:04.0f}", arq_amount * 1e9);
            payed_for_kB_str      = fmt::format("{:0.9f}", payed_for_kB);
            payed_for_kB_nano_str = fmt::format("{:04.0f}", payed_for_kB * 1e9);
        }


        mstch::map txd_map {
                {"hash"              , pod_to_hex(hash)},
                {"prefix_hash"       , pod_to_hex(prefix_hash)},
                {"pub_key"           , pod_to_hex(pk)},
                {"tx_fee"            , fee_str},
                {"tx_fee_short"      , fee_short_str},
                {"fee_nano"          , fee_nano_str},
                {"payed_for_kB"      , payed_for_kB_str},
                {"payed_for_kB_nano" , payed_for_kB_nano_str},
                {"sum_inputs"        , arq_amount_to_str(arq_inputs , "{:0.9f}")},
                {"sum_outputs"       , arq_amount_to_str(arq_outputs, "{:0.9f}")},
                {"sum_inputs_short"  , arq_amount_to_str(arq_inputs , "{:0.9f}")},
                {"sum_outputs_short" , arq_amount_to_str(arq_outputs, "{:0.9f}")},
                {"no_inputs"         , static_cast<uint64_t>(input_key_imgs.size())},
                {"no_outputs"        , static_cast<uint64_t>(output_pub_keys.size())},
                {"no_nonrct_inputs"  , num_nonrct_inputs},
                {"mixin"             , mixin_str},
                {"blk_height"        , blk_height},
                {"version"           , static_cast<uint64_t>(version)},
                {"has_payment_id"    , payment_id  != null_hash},
                {"has_payment_id8"   , payment_id8 != null_hash8},
                {"pID"               , string {pID}},
                {"payment_id"        , pod_to_hex(payment_id)},
                {"confirmations"     , no_confirmations},
                {"extra"             , get_extra_str()},
                {"payment_id8"       , pod_to_hex(payment_id8)},
                {"unlock_time"       , unlock_time},
                {"tx_size"           , fmt::format("{:0.4f}", tx_size)},
                {"tx_size_short"     , fmt::format("{:0.4f}", tx_size)},
                {"has_add_pks"       , !additional_pks.empty()}
        };


        return txd_map;
    }


    string
    get_extra_str() const
    {
        return epee::string_tools::buff_to_hex_nodelimer(string{reinterpret_cast<const char*>(extra.data()), extra.size()});
    }


    mstch::array
    get_ring_sig_for_input(uint64_t in_i)
    {
        mstch::array ring_sigs {};

        if (in_i >= signatures.size())
        {
            return ring_sigs;
        }

        for (const crypto::signature &sig: signatures.at(in_i))
        {
            ring_sigs.push_back(mstch::map{
                    {"ring_sig", print_signature(sig)}
            });
        }

        return ring_sigs;
    }

    string
    print_signature(const signature &sig)
    {
        stringstream ss;

        ss << epee::string_tools::pod_to_hex(sig.c)
           << epee::string_tools::pod_to_hex(sig.r);

        return ss.str();
    }

    ~tx_details() {};
};

struct ServiceNodeContext
{
  std::string html_context;
  std::string html_full_context;
  int num_entries_on_front_page;
};

class page
{

static const bool FULL_AGE_FORMAT {true};

MicroCore* mcore;
Blockchain* core_storage;
ServiceNodeContext snode_context;
rpccalls rpc;

atomic<time_t> server_timestamp;

cryptonote::network_type nettype;
bool mainnet;
bool testnet;
bool stagenet;

bool enable_pusher;

bool enable_key_image_checker;
bool enable_output_key_checker;
bool enable_mixins_details;
bool enable_as_hex;
bool enable_mixin_guess;

bool enable_autorefresh_option;

uint64_t no_of_mempool_tx_of_frontpage;
uint64_t no_blocks_on_index;
uint64_t mempool_info_timeout;
uint64_t no_quorum_entries_on_frontpage;
uint32_t no_checkpoint_entries_on_frontpage;

string testnet_url;
string stagenet_url;
string mainnet_url;

// instead of constatnly reading template files
// from hard drive for each request, we can read
// them only once, when the explorer starts into this map
// this will improve performance of the explorer and reduce
// read operation in OS
map<string, string> template_file;

public:

page(MicroCore* _mcore,
     Blockchain* _core_storage,
     string _daemon_url,
     cryptonote::network_type _nettype,
     bool _enable_pusher,
     bool _enable_as_hex,
     bool _enable_key_image_checker,
     bool _enable_output_key_checker,
     bool _enable_autorefresh_option,
     bool _enable_mixins_details,
     bool _enable_mixin_guess,
     uint64_t _no_blocks_on_index,
     uint64_t _mempool_info_timeout,
     string _testnet_url,
     string _stagenet_url,
     string _mainnet_url)
        : mcore {_mcore},
          core_storage {_core_storage},
          rpc {_daemon_url},
          server_timestamp {std::time(nullptr)},
          nettype {_nettype},
          enable_pusher {_enable_pusher},
          enable_as_hex {_enable_as_hex},
          enable_key_image_checker {_enable_key_image_checker},
          enable_output_key_checker {_enable_output_key_checker},
          enable_autorefresh_option {_enable_autorefresh_option},
          enable_mixins_details {_enable_mixins_details},
          enable_mixin_guess {_enable_mixin_guess},
          no_blocks_on_index {_no_blocks_on_index},
          mempool_info_timeout {_mempool_info_timeout},
          testnet_url {_testnet_url},
          stagenet_url {_stagenet_url},
          mainnet_url {_mainnet_url}
{
    mainnet = nettype == cryptonote::network_type::MAINNET;
    testnet = nettype == cryptonote::network_type::TESTNET;
    stagenet = nettype == cryptonote::network_type::STAGENET;

    snode_context = {};
    snode_context.num_entries_on_front_page = 10;

    no_quorum_entries_on_frontpage = 1;
    no_checkpoint_entries_on_frontpage = 3;

    no_of_mempool_tx_of_frontpage = 25;

    // read template files for all the pages
    // into template_file map

    template_file["css_styles"]          = xmreg::read(TMPL_CSS_STYLES);
    template_file["header"]              = xmreg::read(TMPL_HEADER);
    template_file["footer"]              = get_footer();
    template_file["index2"]              = get_full_page(xmreg::read(TMPL_INDEX2));
    template_file["mempool"]             = xmreg::read(TMPL_MEMPOOL);
    template_file["altblocks"]           = get_full_page(xmreg::read(TMPL_ALTBLOCKS));
    template_file["mempool_error"]       = xmreg::read(TMPL_MEMPOOL_ERROR);
    template_file["mempool_full"]        = get_full_page(template_file["mempool"]);
    template_file["service_nodes"]       = xmreg::read(TMPL_SERVICE_NODES);
    template_file["quorum_states"]       = xmreg::read(TMPL_QUORUM_STATES);
    template_file["quorum_states_full"]  = get_full_page(template_file["quorum_states"]);
    template_file["checkpoints"]         = xmreg::read(TMPL_CHECKPOINTS);
    template_file["checkpoints_full"]    = get_full_page(template_file["checkpoints"]);
    template_file["service_nodes_full"]  = get_full_page(xmreg::read(TMPL_SERVICE_NODES));
    template_file["service_node_detail"] = get_full_page(xmreg::read(TMPL_SERVICE_NODES_DETAIL));
    template_file["block"]               = get_full_page(xmreg::read(TMPL_BLOCK));
    template_file["tx"]                  = get_full_page(xmreg::read(TMPL_TX));
    template_file["my_outputs"]          = get_full_page(xmreg::read(TMPL_MY_OUTPUTS));
    template_file["rawtx"]               = get_full_page(xmreg::read(TMPL_MY_RAWTX));
    template_file["checkrawtx"]          = get_full_page(xmreg::read(TMPL_MY_CHECKRAWTX));
    template_file["pushrawtx"]           = get_full_page(xmreg::read(TMPL_MY_PUSHRAWTX));
    template_file["rawkeyimgs"]          = get_full_page(xmreg::read(TMPL_MY_RAWKEYIMGS));
    template_file["rawoutputkeys"]       = get_full_page(xmreg::read(TMPL_MY_RAWOUTPUTKEYS));
    template_file["checkrawkeyimgs"]     = get_full_page(xmreg::read(TMPL_MY_CHECKRAWKEYIMGS));
    template_file["checkoutputkeys"]     = get_full_page(xmreg::read(TMPL_MY_CHECKRAWOUTPUTKEYS));
    template_file["address"]             = get_full_page(xmreg::read(TMPL_ADDRESS));
    template_file["search_results"]      = get_full_page(xmreg::read(TMPL_SEARCH_RESULTS));
    template_file["tx_details"]          = xmreg::read(string(TMPL_PARTIALS_DIR) + "/tx_details.html");
    template_file["tx_table_head"]       = xmreg::read(string(TMPL_PARTIALS_DIR) + "/tx_table_header.html");
    template_file["tx_table_row"]        = xmreg::read(string(TMPL_PARTIALS_DIR) + "/tx_table_row.html");

    template_file["site_manifest"]       = xmreg::read(TMPL_MANIFEST);
}

std::string portions_to_percent(uint64_t portions)
{
  std::ostringstream os;
  os << portions / (double)STAKING_SHARE_PARTS * 100.;
  return os.str();
}

time_t calculate_service_node_expiry_timestamp(uint64_t expiry_height)
{
  uint64_t curr_height = core_storage->get_current_blockchain_height();

  int64_t delta_height = expiry_height - curr_height;
  time_t result = time(nullptr) + (delta_height * DIFFICULTY_TARGET_V16);
  return result;
}

void set_service_node_fields(mstch::map &context, const cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response::entry &entry)
{
  std::ostringstream num_contributors;
  num_contributors << entry.contributors.size() << "/" << MAX_NUMBER_OF_CONTRIBUTORS;

  uint64_t open_contribution_remaining = entry.staking_requirement > entry.total_reserved ? entry.staking_requirement - entry.total_reserved : 0;

  std::string expiration_time_relative;
  std::string expiration_time_str = make_service_node_expiry_time_str(&entry, &expiration_time_relative);

  context["public_key"] = entry.service_node_pubkey;
  context["num_contributors"] = num_contributors.str();
  context["operator_cut"] = portions_to_percent(entry.portions_for_operator);

  if (entry.portions_for_operator == STAKING_SHARE_PARTS)
    context["solo_node"] = true;

  context["operator_address"] = entry.operator_address;
  context["open_for_contribution"] = print_money(open_contribution_remaining);
  if (!open_contribution_remaining && !entry.funded)
    context["only_reserved_spots"] = true;

  context["total_contributed"] = print_money(entry.total_contributed);
  context["total_reserved"] = print_money(entry.total_reserved);
  context["staking_requirement"] = print_money(entry.staking_requirement);
  context["stake_remaining"] = print_money(entry.funded ? 0 : entry.staking_requirement - entry.total_contributed);
  context["stake_min_contribution"] = print_money(!entry.funded && entry.contributors.size() < MAX_NUMBER_OF_CONTRIBUTORS
          ? open_contribution_remaining / (MAX_NUMBER_OF_CONTRIBUTORS - entry.contributors.size())
          : 0);
  if (entry.funded)
    context["is_fully_funded"] = true;

  context["register_height"] = entry.registration_height;
  context["last_reward_at_block"] = entry.last_reward_block_height;
  if (entry.last_reward_transaction_index < std::numeric_limits<uint32_t>::max())
    context["last_contribution_tx_index"] = entry.last_reward_transaction_index;

  if (entry.active)
    context["activation_height"] = entry.state_height;
  else if (!entry.funded)
    context["awaiting"] = true;
  else
    context["decommission_height"] = entry.state_height;

  if (entry.requested_unlock_height > 0)
  {
    context["expiration_block"] = entry.requested_unlock_height;
    context["expiration_date"] = expiration_time_str;
    context["expiration_time_relative"] = expiration_time_relative;
  }

  context["last_uptime_proof"] = last_uptime_proof_to_string(entry.last_uptime_proof);
  context["earned_downtime_blocks"] = entry.earned_downtime_blocks;
  if (entry.earned_downtime_blocks > 0)
  {
    context["credit_remaining"] = entry.earned_downtime_blocks;
    context["earned_downtime"] = get_human_timespan(DIFFICULTY_TARGET_V16 * entry.earned_downtime_blocks);
    if (entry.active && entry.earned_downtime_blocks < service_nodes::DECOMMISSION_MINIMUM)
      context["earned_downtime_below_min"] = true;
  }

  auto &contributors = boost::get<mstch::array>(context.emplace("service_node_contributors", mstch::array{}).first->second);
  for (const auto &contributor : entry.contributors)
    contributors.push_back(mstch::map{
      {"address",  contributor.address},
      {"amount",   print_money(contributor.amount)},
      {"reserved", print_money(contributor.reserved)},
    });
}

void generate_service_node_mapping(mstch::map &context, std::string name, bool on_homepage, std::vector<cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response::entry *> const &entries)
{
  size_t iterate_count = on_homepage ? snode_context.num_entries_on_front_page : entries.size();
  iterate_count = std::min(entries.size(), iterate_count);

  auto &array = boost::get<mstch::array>(context.emplace(name, mstch::array()).first->second);
  array.reserve(iterate_count);

  for (size_t i = 0; i < iterate_count; ++i)
  {
    mstch::map array_entry;
    set_service_node_fields(array_entry, *entries[i]);
    array.push_back(std::move(array_entry));
  }

  context[name + "_size"] = entries.size();
  size_t more = entries.size() - array.size();
  if (more)
    context[name + "_more"] = more;
}

std::string
render_service_nodes_html(bool add_header_and_footer)
{
  bool on_homepage = !add_header_and_footer;

  COMMAND_RPC_GET_SERVICE_NODES::response response;
  if (!rpc.get_service_node(response, {}))
  {
    return (on_homepage) ? snode_context.html_context : snode_context.html_full_context;
  }

  using sn_entry = cryptonote::COMMAND_RPC_GET_SERVICE_NODES::response::entry;

  std::vector<sn_entry *> active, inactive, awaiting, reserved;
  active.reserve(response.service_node_states.size());

  for (auto &entry : response.service_node_states)
  {
    if (entry.active)
      active.push_back(&entry);
    else if (entry.funded)
      inactive.push_back(&entry);
    else
      awaiting.push_back(&entry);
  }

  std::sort(active.begin(), active.end(), [](const sn_entry *a, const sn_entry *b) {
    return std::make_tuple(a->last_reward_block_height, a->last_reward_transaction_index, a->service_node_pubkey)
         < std::make_tuple(b->last_reward_block_height, b->last_reward_transaction_index, b->service_node_pubkey); });

  std::sort(inactive.begin(), inactive.end(), [](const sn_entry *a, const sn_entry *b) {
    return std::make_tuple(std::max(a->earned_downtime_blocks, int64_t{0}), a->state_height, a->last_uptime_proof, a->service_node_pubkey)
         < std::make_tuple(std::max(b->earned_downtime_blocks, int64_t{0}), b->state_height, b->last_uptime_proof, b->service_node_pubkey); });

  std::sort(awaiting.begin(), awaiting.end(), [](const sn_entry *a, const sn_entry *b) {
    return std::make_tuple(a->portions_for_operator, a->staking_requirement - a->total_reserved, a->staking_requirement - a->total_contributed, a->service_node_pubkey)
         < std::make_tuple(b->portions_for_operator, b->staking_requirement - b->total_reserved, b->staking_requirement - b->total_contributed, b->service_node_pubkey); });

  mstch::map page_context;
  generate_service_node_mapping(page_context, "service_nodes_active", on_homepage, active);
  generate_service_node_mapping(page_context, "service_nodes_inactive", on_homepage, inactive);
  generate_service_node_mapping(page_context, "service_nodes_awaiting", on_homepage, awaiting);

  if (on_homepage)
  {
    snode_context.html_context = mstch::render(template_file["service_nodes"], page_context);
    return snode_context.html_context;
  }
  else
  {
    add_css_style(page_context);
    snode_context.html_full_context = mstch::render(template_file["service_nodes_full"], page_context);
    return snode_context.html_full_context;
  }
}

std::string last_uptime_proof_to_string(time_t uptime_proof)
{
  static std::string friendly_uptime_proof_not_received = "Not Received";

  if (uptime_proof == 0)
  {
    return friendly_uptime_proof_not_received;
  }
  else
  {
    return get_age(server_timestamp, uptime_proof).first;
  }
}

using sn_entry_map = std::unordered_map<std::string, COMMAND_RPC_GET_SERVICE_NODES::response::entry>;

static bool service_node_entry_is_infinite_staking(COMMAND_RPC_GET_SERVICE_NODES::response::entry const *entry)
{
  bool result = entry->contributors[0].locked_contributions.size() > 0;
  return result;
}

std::string make_service_node_expiry_time_str(COMMAND_RPC_GET_SERVICE_NODES::response::entry const *entry, std::string *expiry_time_relative)
{
  std::string result;
  uint64_t expiry_height = 0;

  if (entry->contributors[0].locked_contributions.size() > 0)
    expiry_height = entry->requested_unlock_height;
  else
    expiry_height = entry->registration_height + service_nodes::staking_num_lock_blocks(nettype);

  if (expiry_height > 0)
  {
    time_t expiry_time = calculate_service_node_expiry_timestamp(expiry_height);
    get_human_readable_timestamp(expiry_time, &result);
    if (expiry_time_relative)
      *expiry_time_relative = get_human_time_ago(expiry_time, time(nullptr));
  }
  else
  {
    if (expiry_time_relative)
      *expiry_time_relative = "N/A";

    result = "Staking Infinitely";
  }

  return result;
}

mstch::array gather_sn_data(const std::vector<std::string> &nodes, const sn_entry_map &sn_map, const size_t sn_display_limit)
{
  mstch::array data;
  data.reserve(nodes.size());
  static const std::string failed_entry = "--";

  const size_t max = std::min(sn_display_limit, nodes.size());
  for (size_t i = 0; i < max; i++)
  {
    const auto &pk_str = nodes[i];
    mstch::map array_entry{{"public_key", pk_str}, {"quorum_index", std::to_string(i)}};

    auto it = sn_map.find(pk_str);

    if (it == sn_map.end())
    {
      array_entry.emplace("last_uptime_proof", failed_entry);
      array_entry.emplace("expiration_date", failed_entry);
      array_entry.emplace("expiration_time_relative", failed_entry);
    }
    else
    {
      std::string expiration_time_relative;
      std::string expiration_time_str = make_service_node_expiry_time_str(&it->second, &expiration_time_relative);

      array_entry.emplace("last_uptime_proof", last_uptime_proof_to_string(it->second.last_uptime_proof));
      array_entry.emplace("expiration_date", expiration_time_str);
      array_entry.emplace("expiration_time_relative", expiration_time_relative);
    }
    data.push_back(std::move(array_entry));
  }

  return data;
}

mstch::map get_quorum_state_context(uint64_t start_height, uint64_t end_height, size_t num_quorums, size_t sn_display_limit = 20, const service_nodes::quorum_type *type = nullptr)
{
  uint8_t q_type = type ? static_cast<uint8_t>(*type) : COMMAND_RPC_GET_QUORUM_STATE::ALL_QUORUMS_SENTINEL_VALUE;
  COMMAND_RPC_GET_QUORUM_STATE::response response = {};
  rpc.get_quorum_state(response, start_height, end_height, q_type);

  sn_entry_map pk2sninfo;
  {
    COMMAND_RPC_GET_SERVICE_NODES::response sn_response = {};
    rpc.get_service_node(sn_response, {});

    for (const auto& entry : sn_response.service_node_states)
      pk2sninfo.emplace(entry.service_node_pubkey, entry);
  }

  std::vector<std::pair<service_nodes::quorum_type, std::string>> quorum_types;
  if (!type || *type == service_nodes::quorum_type::obligations)
    quorum_types.emplace_back(service_nodes::quorum_type::obligations, "obligations");
  if (!type || *type == service_nodes::quorum_type::checkpointing)
    quorum_types.emplace_back(service_nodes::quorum_type::checkpointing, "checkpointing");

  mstch::map page_context {};

  for (const auto &quorum_type : quorum_types)
  {
    mstch::array quorum_array;
    quorum_array.reserve(num_quorums);
    page_context.emplace(quorum_type.second + "_quorum_array", std::move(quorum_array));
  }

  for (const auto &quorum_type : quorum_types)
  {
    auto &quorum_array = boost::get<mstch::array>(page_context[quorum_type.second + "_quorum_array"]);
    for (const auto &entry : response.quorums)
    {
      auto group_display_limit = sn_display_limit;
      auto qt = static_cast<service_nodes::quorum_type>(entry.quorum_type);
      if (qt != quorum_type.first)
        continue;

      mstch::map quorum_part;
      quorum_part.emplace("quorum_height", entry.height);
      auto validators = gather_sn_data(entry.quorum.validators, pk2sninfo, group_display_limit);
      quorum_part.emplace("quorum_validators_size", entry.quorum.validators.size());
      if (validators.size() < entry.quorum.validators.size())
        quorum_part.emplace("quorum_validators_more", entry.quorum.validators.size() - validators.size());
      group_display_limit -= validators.size();
      quorum_part.emplace("quorum_validators", std::move(validators));

      auto workers = gather_sn_data(entry.quorum.workers, pk2sninfo, group_display_limit);
      quorum_part.emplace("quorum_workers_size", entry.quorum.workers.size());
      if (workers.size() < entry.quorum.workers.size())
        quorum_part.emplace("quorum_workers_more", entry.quorum.workers.size() - workers.size());
      quorum_part.emplace("quorum_workers", std::move(workers));

      if (qt == service_nodes::quorum_type::checkpointing)
        quorum_part.emplace("quorum_block_height", entry.height - service_nodes::REORG_SAFETY_BUFFER_IN_BLOCKS);

      quorum_array.push_back(std::move(quorum_part));

      if (quorum_array.size() >= num_quorums)
        break;
    }
    std::reverse(quorum_array.begin(), quorum_array.end());
    page_context.emplace(quorum_type.second + "_quorum_array_size", quorum_array.size());
  }
  return page_context;
}

std::string render_single_quorum_html(service_nodes::quorum_type qtype, uint64_t height)
{
  auto page_context = get_quorum_state_context(height, height, 1, std::numeric_limits<size_t>::max(), &qtype);
  add_css_style(page_context);
  return mstch::render(template_file["quorum_states_full"], page_context);
}

std::string render_quorum_states_html(bool add_header_and_footer)
{
  bool on_homepage = !add_header_and_footer;
  size_t num_quorums_to_render = on_homepage ? no_quorum_entries_on_frontpage : 30;

  uint64_t block_height = core_storage->get_current_blockchain_height() - 1;
  uint64_t start_height = block_height <= num_quorums_to_render ? 0 : block_height - num_quorums_to_render - 1;
  uint64_t end_height = block_height + 3; // checkpoint quorum is every 4 blocks

  auto page_context = get_quorum_state_context(start_height, end_height, num_quorums_to_render);

  if (on_homepage)
    return mstch::render(template_file["quorum_states"], page_context);

  add_css_style(page_context);
  return mstch::render(template_file["quorum_states_full"], page_context);
}

std::string render_checkpoints_html(bool add_header_and_footer)
{
  bool on_homepage = !add_header_and_footer;
  uint32_t num_checkpoints = on_homepage ? no_checkpoint_entries_on_frontpage : 50;

  COMMAND_RPC_GET_CHECKPOINTS::response response = {};
  rpc.get_checkpoints(response, num_checkpoints);

  mstch::array checkpoints;
  checkpoints.reserve(response.checkpoints.size());
  for (const auto &cp : response.checkpoints)
  {
    mstch::map checkpoint;
    checkpoint.emplace("checkpoint_type", cp.type);
    checkpoint.emplace("checkpoint_block_hash", cp.block_hash);
    checkpoint.emplace("checkpoint_height", cp.height);
    checkpoint.emplace("checkpoint_quorum", cp.height - service_nodes::REORG_SAFETY_BUFFER_IN_BLOCKS);
    mstch::array signatures;
    signatures.reserve(cp.signatures.size());
    mstch::array voter_signed;
    voter_signed.reserve(service_nodes::CHECKPOINT_QUORUM_SIZE);
    for (size_t i = 0; i < service_nodes::CHECKPOINT_QUORUM_SIZE; i++)
      voter_signed.push_back(mstch::map{{"voter_index", i}});
    for (const auto &s : cp.signatures)
    {
      mstch::map sig_info;
      sig_info.emplace("checkpoint_signer_voter_index", s.voter_index);
      sig_info.emplace("checkpoint_signature", s.signature);
      signatures.push_back(std::move(sig_info));
      boost::get<mstch::map>(voter_signed[s.voter_index]).emplace("voter_signed", true);
    }
    checkpoint.emplace("checkpoint_signatures_size", signatures.size());
    checkpoint.emplace("checkpoint_signatures", std::move(signatures));
    checkpoint.emplace("checkpoint_signed_by", std::move(voter_signed));

    checkpoints.push_back(std::move(checkpoint));
  }

  mstch::map page_context = {};
  page_context.emplace("checkpoint_array_size", checkpoints.size());
  page_context.emplace("checkpoint_array", std::move(checkpoints));

  if (on_homepage)
    return mstch::render(template_file["checkpoints"], page_context);

  add_css_style(page_context);
  return mstch::render(template_file["checkpoints_full"], page_context);
}

void add_tx_metadata(mstch::map &context, const cryptonote::transaction &tx, bool detailed = false)
{
  bool is_miner_tx = (tx.vin.size() && tx.vin[0].type() == typeid(cryptonote::txin_gen));
  context["is_miner_tx"] = is_miner_tx;
  if (is_miner_tx)
  {
    if (detailed)
    {
      crypto::public_key winner = cryptonote::get_service_node_winner_from_tx_extra(tx.extra);
      context["service_node_winner"] = pod_to_hex(winner);
    }
  }

  if (tx.version < cryptonote::txversion::v3)
    return;

  tx_extra_service_node_register register_;
  tx_extra_tx_key_image_unlock unlock_;
  cryptonote::account_public_address contributor;
  if (tx.tx_type == cryptonote::txtype::state_change)
  {
    tx_extra_service_node_state_change state_change;
    context["is_state_change"] = true;
    if (get_service_node_state_change_from_tx_extra(tx.extra, state_change))
    {
      context["state_change_service_node_index"] = state_change.service_node_index;
      context["state_change_block_height"] = state_change.block_height;
      context[
              state_change.state == service_nodes::new_state::deregister ? "state_change_deregister" :
              state_change.state == service_nodes::new_state::decommission ? "state_change_decommission" :
              state_change.state == service_nodes::new_state::recommission ? "state_change_recommission" :
              state_change.state == service_nodes::new_state::ip_change_penalty ? "state_change_ip_change_penalty" :
              "state_change_unknown"] = true;

      if (detailed)
      {
        std::vector<std::string> quorum_nodes;
        COMMAND_RPC_GET_QUORUM_STATE::response response = {};
        rpc.get_quorum_state(response, state_change.block_height, state_change.block_height, static_cast<uint8_t>(service_nodes::quorum_type::obligations));

        if (response.status == "OK" && !response.quorums.empty())
        {
          auto &quorum = response.quorums[0].quorum;
          if (state_change.service_node_index < quorum.workers.size())
            context["state_change_service_node_pubkey"] = quorum.workers[state_change.service_node_index];
          quorum_nodes = std::move(quorum.validators);
          context["state_change_have_pubkey_info"] = true;
        }

        auto &vote_array = get<mstch::array>(context.emplace("state_change_vote_array", mstch::array()).first->second);
        vote_array.reserve(state_change.votes.size());

        for (const auto &vote : state_change.votes)
        {
          mstch::map entry
          {
            {"state_change_voters_quorum_index", vote.validator_index},
            {"state_change_signature", pod_to_hex(vote.signature)},
          };
          if (vote.validator_index < quorum_nodes.size())
            entry["state_change_voter_pubkey"] = quorum_nodes[vote.validator_index];

          vote_array.push_back(std::move(entry));
        }
      }
    }
    else
    {
      context["state_change_unknown"] = std::string{"unknown"};
    }
  }
  else if (tx.tx_type == cryptonote::txtype::key_image_unlock && get_tx_key_image_unlock_from_tx_extra(tx.extra, unlock_))
  {
    context["have_unlock_info"] = true;
    context["unlock_service_node_pubkey"] = extract_sn_pubkey(tx.extra);
    if (detailed)
    {
      context["unlock_key_image"] = pod_to_hex(unlock_.key_image);
      context["unlock_signature"] = pod_to_hex(unlock_.signature);
    }
  }
  else if (get_service_node_register_from_tx_extra(tx.extra, register_))
  {
    context["have_register_info"] = true;
    context["register_service_node_pubkey"] = extract_sn_pubkey(tx.extra);
    context["register_portions_for_operator"] = portions_to_percent(register_.m_portions_for_operator);

    if (detailed)
    {
      context["register_expiration_timestamp_friendly"]  = timestamp_to_str_gm(register_.m_expiration_timestamp);
      context["register_expiration_timestamp_relative"] = get_human_time_ago(register_.m_expiration_timestamp, time(nullptr));
      context["register_expiration_timestamp"] = register_.m_expiration_timestamp;
      context["register_signature"] = pod_to_hex(register_.m_service_node_signature);

      auto &array = get<mstch::array>(context.emplace("register_array", mstch::array()).first->second);
      array.reserve(register_.m_public_spend_keys.size());

      for (size_t i = 0; i < register_.m_public_spend_keys.size(); ++i)
      {
        crypto::public_key const &spend_key = register_.m_public_spend_keys[i];
        crypto::public_key const &view_key =  register_.m_public_view_keys[i];
        auto portion = register_.m_portions[i];

        account_public_address address = {};
        address.m_spend_public_key     = spend_key;
        address.m_view_public_key      = view_key;

        mstch::map entry
        {
          {"register_address", get_account_address_as_str(nettype, false , address)},
          {"register_portions", portions_to_percent(portion)},
        };

        array.push_back(entry);
      }
    }
  }
  else if (get_service_node_contributor_from_tx_extra(tx.extra, contributor))
  {
    context["have_contribution_info"] = true;
    context["contribution_service_node_pubkey"] = extract_sn_pubkey(tx.extra);
    context["contribution_address"] = get_account_address_as_str(nettype, false, contributor);

    uint64_t amount = get_amount_from_stake(tx, contributor);
    context["contribution_amount"] = amount > 0 ? xmreg::arq_amount_to_str(amount, "{:0.9f}", true) : "<decode error>";
  }
}

/**
 * @brief show recent transactions and mempool
 * @param page_no block page to show
 * @param refresh_page enable autorefresh
 * @return rendered index page
 */
string
index2(uint64_t page_no = 0, bool refresh_page = false)
{

    // we get network info, such as current hash rate
    // but since this makes a rpc call to daemon, we make it as an async
    // call. this way we dont have to wait with execution of the rest of the
    // index2 method, until daemon gives as the required result.
    std::future<json> network_info_ftr = std::async(std::launch::async, [&]
    {
        json j_info;

        get_arqma_network_info(j_info);

        return j_info;
    });


    // get mempool for the front page also using async future
    std::future<string> mempool_ftr = std::async(std::launch::async, [&]
    {
        // get memory pool rendered template
        return mempool(false, no_of_mempool_tx_of_frontpage);
    });

    std::vector<std::pair<std::string, std::future<std::string>>> summary_futures;
    summary_futures.emplace_back("service_node_summary", std::async(std::launch::async, [&] { return render_service_nodes_html(false); }));
    summary_futures.emplace_back("quorum_state_summary", std::async(std::launch::async, [&] { return render_quorum_states_html(false); }));
    summary_futures.emplace_back("checkpoint_summary", std::async(std::launch::async, [&] { return render_checkpoints_html(false); }));

    //get current server timestamp
    server_timestamp = std::time(nullptr);

    uint64_t local_copy_server_timestamp = server_timestamp;

    // get the current blockchain height. Just to check
    uint64_t height = core_storage->get_current_blockchain_height();

    // number of last blocks to show
    uint64_t no_of_last_blocks = std::min(no_blocks_on_index + 1, height);

    // initalise page template map with basic info about blockchain
    mstch::map context {
            {"testnet"                  , testnet},
            {"stagenet"                 , stagenet},
            {"testnet_url"              , testnet_url},
            {"stagenet_url"             , stagenet_url},
            {"mainnet_url"              , mainnet_url},
            {"refresh"                  , refresh_page},
            {"height"                   , height},
            {"server_timestamp"         , xmreg::timestamp_to_str_gm(local_copy_server_timestamp)},
            {"age_format"               , string("[h:m:d]")},
            {"page_no"                  , page_no},
            {"total_page_no"            , (height / no_of_last_blocks)},
            {"is_page_zero"             , !bool(page_no)},
            {"no_of_last_blocks"        , no_of_last_blocks},
            {"next_page"                , (page_no + 1)},
            {"prev_page"                , (page_no > 0 ? page_no - 1 : 0)},
            {"enable_pusher"            , enable_pusher},
            {"enable_key_image_checker" , enable_key_image_checker},
            {"enable_output_key_checker", enable_output_key_checker},
            {"enable_autorefresh_option", enable_autorefresh_option}
    };

    context.emplace("txs", mstch::array()); // will keep tx to show

    // get reference to txs mstch map to be field below
    mstch::array& txs = boost::get<mstch::array>(context["txs"]);

    // calculate starting and ending block numbers to show
    int64_t start_height = height - no_of_last_blocks * (page_no + 1);

    // check if start height is not below range
    start_height = start_height < 0 ? 0 : start_height;

    int64_t end_height = start_height + no_of_last_blocks - 1;

    vector<double> blk_sizes;

    // loop index
    int64_t i = end_height;

    // iterate over last no_of_last_blocks of blocks
    while (i >= start_height)
    {
        // get block at the given height i
        block blk;

        if (!mcore->get_block_by_height(i, blk))
        {
            cerr << "Cant get block: " << i << endl;
            --i;
            continue;
        }

        // get block's hash
        crypto::hash blk_hash = core_storage->get_block_id_by_height(i);

        // get block size in kB
        double blk_size = static_cast<double>(core_storage->get_db().get_block_weight(i))/1024.0;

        string blk_size_str = fmt::format("{:0.4f}", blk_size);

        blk_sizes.push_back(blk_size);

        // remove "<" and ">" from the hash string
        string blk_hash_str = pod_to_hex(blk_hash);

        // get block age
        pair<string, string> age = get_age(local_copy_server_timestamp, blk.timestamp);

        context["age_format"] = age.second;

        uint64_t blk_diff = core_storage->get_db().get_block_difficulty(i);

        // start measure time
        auto start = std::chrono::steady_clock::now();

        // Get all transactions in the block found, Initialize the first list with
        // transaction for solving the block, eg: coinbase
        vector<cryptonote::transaction> blk_txs {blk.miner_tx};
        vector<crypto::hash> missed_txs;

        if(!core_storage->get_transactions(blk.tx_hashes, blk_txs, missed_txs))
        {
          cerr << "Can't get transactions in block: " << i << endl;
          --i;
          continue;
        }

        uint64_t tx_i {0};

        //          tx_hash     , txd_map
        vector<pair<crypto::hash, mstch::node>> txd_pairs;

        for(auto it = blk_txs.begin(); it != blk_txs.end(); ++it)
        {
            const cryptonote::transaction& tx = *it;

            const tx_details& txd = get_tx_details(tx, false, i, height);

            mstch::map txd_map = txd.get_mstch_map();

            // add age to the txd mstch map
            txd_map.insert({"height"    , i});
            txd_map.insert({"blk_hash"  , blk_hash_str});
            txd_map.insert({"age"       , age.first});
            txd_map.insert({"is_ringct" , tx.version >= cryptonote::txversion::v2});
            txd_map.insert({"rct_type"  , tx.rct_signatures.type});
            txd_map.insert({"blk_size"  , blk_size_str});

            // do not show block info for other than 1st tx.
            if(tx_i > 0)
            {
                txd_map["height"]	= string("");
                txd_map["age"]		= string("");
                txd_map["blk_size"]	= string("");
            }

            add_tx_metadata(txd_map, tx);
            txd_pairs.emplace_back(txd.hash, txd_map);

            ++tx_i;

        } // for(list<cryptonote::transaction>::reverse_iterator rit = blk_txs.rbegin();

        // copy tx maps from txs_maps_tmp into txs array, that will go to templates
        for(const pair<crypto::hash, mstch::node>& txd_pair : txd_pairs)
        {
            txs.push_back(boost::get<mstch::map>(txd_pair.second));
        }

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);

        --i; // go to next block number

    } // while (i <= end_height)

    // calculate median size of the blocks shown
    //double blk_size_median = xmreg::calc_median(blk_sizes.begin(), blk_sizes.end());

    // get current network info from MemoryStatus thread.
    MempoolStatus::network_info current_network_info = MempoolStatus::current_network_info;

    // perapre network info mstch::map for the front page
    string hash_rate;

    if(current_network_info.hash_rate > 1e6)
        hash_rate = fmt::format("{:0.3f} MH/s", current_network_info.hash_rate/1.0e6);
    else if(current_network_info.hash_rate > 1e3)
        hash_rate = fmt::format("{:0.3f} kH/s", current_network_info.hash_rate/1.0e3);
    else
        hash_rate = fmt::format("{:d} H/s", current_network_info.hash_rate);

    pair<string, string> network_info_age = get_age(local_copy_server_timestamp, current_network_info.info_timestamp);

    // if network info is younger than 2 minute, assume its current. No sense
    // showing that it is not current if its less then block time.
    if (local_copy_server_timestamp - current_network_info.info_timestamp < 120)
    {
        current_network_info.current = true;
    }

    std::string fee_type = height >= 248200 ? "byte" : "kB";

    context["network_info"] = mstch::map {
            {"difficulty"         , current_network_info.difficulty},
            {"hash_rate"          , hash_rate},
            {"fee_type"           , fee_type},
            {"fee_per_kb"         , print_money(current_network_info.fee_per_kb)},
            {"alt_blocks_no"      , current_network_info.alt_blocks_count},
            {"have_alt_block"     , (current_network_info.alt_blocks_count > 0)},
            {"tx_pool_size"       , current_network_info.tx_pool_size},
            {"block_size_limit"   , string {current_network_info.block_size_limit_str}},
            {"block_size_median"  , string {current_network_info.block_size_median_str}},
            {"is_current_info"    , current_network_info.current},
            {"is_pool_size_zero"  , (current_network_info.tx_pool_size == 0)},
            {"current_hf_version" , current_network_info.current_hf_version},
            {"staking_requirement", print_money(current_network_info.staking_requirement)},
            {"age"                , network_info_age.first},
            {"age_format"         , network_info_age.second},
    };

    // median size of 100 blocks
    context["blk_size_median"] = string {current_network_info.block_size_median_str};

    string mempool_html {"Cant get mempool_pool"};

    // get mempool data for the front page, if ready. If not, then just skip.
    std::future_status mempool_ftr_status = mempool_ftr.wait_for(std::chrono::milliseconds(mempool_info_timeout));

    if (mempool_ftr_status == std::future_status::ready)
    {
        mempool_html = mempool_ftr.get();
    }
    else
    {
        cerr << "mempool future not ready yet, skipping." << endl;
        mempool_html = mstch::render(template_file["mempool_error"], context);
    }

    if (CurrentBlockchainStatus::is_thread_running())
    {
        CurrentBlockchainStatus::Emission current_values = CurrentBlockchainStatus::get_emission();

        string emission_blk_no         = std::to_string(current_values.blk_no - 1);
        string emission_coinbase       = arq_amount_to_str(current_values.coinbase, "{:0.9f}");
        string emission_fee            = arq_amount_to_str(current_values.fee, "{:0.9f}");
        string emission_coinbase_human = fmt::format("{:n}", static_cast<int64_t>(current_values.coinbase/1e9));
        string emission_fee_human      = fmt::format("{:n}", static_cast<int64_t>(current_values.fee/1e9));

        context["emission"] = mstch::map {
                {"blk_no"            , emission_blk_no},
                {"amount"            , emission_coinbase},
                {"amount_human"      , emission_coinbase_human},
                {"fee_amount"        , emission_fee},
                {"fee_human"         , emission_fee_human}
        };
    }
    else
    {
        cerr  << "emission thread not running, skipping." << endl;
    }

    // Service nodes, quorums, checkpoint summaries:
    auto timeout = std::chrono::system_clock::now() + std::chrono::seconds(1);
    for (auto &sf : summary_futures)
      if (sf.second.wait_until(timeout) == std::future_status::ready)
        context.emplace(std::move(sf.first), sf.second.get());

    // append html files to the index context map
    context["mempool_info"] = mempool_html;

    add_css_style(context);

    // render the page
    return mstch::render(template_file["index2"], context);
}

/**
 * Render mempool data
 */
string
mempool(bool add_header_and_footer = false, uint64_t no_of_mempool_tx = 25)
{
    std::vector<MempoolStatus::mempool_tx> mempool_txs;

    if (add_header_and_footer)
    {
        // get all memmpool txs
        mempool_txs = MempoolStatus::get_mempool_txs();
        no_of_mempool_tx = mempool_txs.size();
    }
    else
    {
        // get only first no_of_mempool_tx txs
        mempool_txs = MempoolStatus::get_mempool_txs(no_of_mempool_tx);
        no_of_mempool_tx = std::min<uint64_t>(no_of_mempool_tx, mempool_txs.size());
    }

    // total size of mempool in bytes
    uint64_t mempool_size_bytes = MempoolStatus::mempool_size;

    // reasign this number, in case no of txs in mempool is smaller
    // than what we requested or we want all txs.


    uint64_t total_no_of_mempool_tx = MempoolStatus::mempool_no;

    // initalise page tempate map with basic info about mempool
    mstch::map context {
            {"mempool_size"          , static_cast<uint64_t>(total_no_of_mempool_tx)}, // total no of mempool txs
            {"mempool_refresh_time"  , MempoolStatus::mempool_refresh_time}
    };

    context.emplace("mempooltxs" , mstch::array());

    // get reference to blocks template map to be field below
    mstch::array& txs = boost::get<mstch::array>(context["mempooltxs"]);

    uint64_t local_copy_server_timestamp = server_timestamp;

    // for each transaction in the memory pool
    for (size_t i = 0; i < no_of_mempool_tx; ++i)
    {
        // get transaction info of the tx in the mempool
        const MempoolStatus::mempool_tx& mempool_tx = mempool_txs.at(i);

        // calculate difference between tx in mempool and server timestamps
        array<size_t, 5> delta_time = timestamp_difference(
                local_copy_server_timestamp,
                mempool_tx.receive_time);

        // use only hours, so if we have days, add
        // it to hours
        uint64_t delta_hours {delta_time[1]*24 + delta_time[2]};

        string age_str = fmt::format("{:02d}:{:02d}:{:02d}",
                                     delta_hours,
                                     delta_time[3], delta_time[4]);

        // if more than 99 hourse, change formating
        // for the template
        if (delta_hours > 99)
        {
            age_str = fmt::format("{:03d}:{:02d}:{:02d}",
                                  delta_hours,
                                  delta_time[3], delta_time[4]);
        }

        // cout << "block_tx_json_cache from cache" << endl;

        // set output page template map
        mstch::map context{
                {"timestamp_no"      , mempool_tx.receive_time},
                {"timestamp"         , mempool_tx.timestamp_str},
                {"age"               , age_str},
                {"hash"              , pod_to_hex(mempool_tx.tx_hash)},
                {"fee"               , mempool_tx.fee_str},
                {"fee_nano"          , mempool_tx.fee_nano_str},
                {"payed_for_kB"      , mempool_tx.payed_for_kB_str},
                {"payed_for_kB_nano" , mempool_tx.payed_for_kB_nano_str},
                {"arq_inputs"        , mempool_tx.arq_inputs_str},
                {"arq_outputs"       , mempool_tx.arq_outputs_str},
                {"no_inputs"         , mempool_tx.no_inputs},
                {"no_outputs"        , mempool_tx.no_outputs},
                {"pID"               , string {mempool_tx.pID}},
                {"no_nonrct_inputs"  , mempool_tx.num_nonrct_inputs},
                {"mixin"             , mempool_tx.mixin_no},
                {"txsize"            , mempool_tx.txsize}
        };

        add_tx_metadata(context, mempool_tx.tx);

        txs.push_back(std::move(context));
    }

    context.insert({"mempool_size_kB", fmt::format("{:0.4f}", static_cast<double>(mempool_size_bytes)/1024.0)});

    if (add_header_and_footer)
    {
        // this is when mempool is on its own page, /mempool
        add_css_style(context);

        context["partial_mempool_shown"] = false;

        // render the page
        return mstch::render(template_file["mempool_full"], context);
    }

    // this is for partial disply on front page.

    context["mempool_fits_on_front_page"]    = (total_no_of_mempool_tx <= mempool_txs.size());
    context["no_of_mempool_tx_of_frontpage"] = no_of_mempool_tx;

    context["partial_mempool_shown"] = true;

    // render the page
    return mstch::render(template_file["mempool"], context);
}


string
altblocks()
{

    // initalise page tempate map with basic info about blockchain
    mstch::map context {
            {"testnet"              , testnet},
            {"stagenet"             , stagenet},
            {"blocks"               , mstch::array()}
    };

    uint64_t local_copy_server_timestamp = server_timestamp;

    // get reference to alt blocks template map to be field below
    mstch::array& blocks = boost::get<mstch::array>(context["blocks"]);

    vector<string> atl_blks_hashes;

    if (!rpc.get_alt_blocks(atl_blks_hashes))
    {
        cerr << "rpc.get_alt_blocks(atl_blks_hashes) failed" << endl;
    }

    context.emplace("no_alt_blocks", (uint64_t)atl_blks_hashes.size());

    for (const string& alt_blk_hash: atl_blks_hashes)
    {
        block alt_blk;
        string error_msg;

        int64_t no_of_txs {-1};
        int64_t blk_height {-1};

        // get block age
        pair<string, string> age {"-1", "-1"};


        if (rpc.get_block(alt_blk_hash, alt_blk, error_msg))
        {
            no_of_txs  = alt_blk.tx_hashes.size();

            blk_height = get_block_height(alt_blk);

            age = get_age(local_copy_server_timestamp, alt_blk.timestamp);
        }

        blocks.push_back(mstch::map {
                {"height"   , blk_height},
                {"age"      , age.first},
                {"hash"     , alt_blk_hash},
                {"no_of_txs", no_of_txs}
        });

    }

    add_css_style(context);


    // render the page
    return mstch::render(template_file["altblocks"], context);
}


string
show_block(uint64_t _blk_height)
{

    // get block at the given height i
    block blk;

    //cout << "_blk_height: " << _blk_height << endl;

    uint64_t current_blockchain_height = core_storage->get_current_blockchain_height();

    if (_blk_height > current_blockchain_height)
    {
        cerr << "Cant get block: " << _blk_height
             << " since its higher than current blockchain height"
             << " i.e., " <<  current_blockchain_height
             << endl;
        return fmt::format("Cant get block {:d} since its higher than current blockchain height!", _blk_height);
    }


    if (!mcore->get_block_by_height(_blk_height, blk))
    {
        cerr << "Cant get block: " << _blk_height << endl;
        return fmt::format("Cant get block {:d}!", _blk_height);
    }

    uint64_t blk_diff;
    if (!mcore->get_diff_at_height(_blk_height, blk_diff))
    {
        cerr << "Cant get block diff: " << _blk_height << endl;
        return fmt::format("Cant get block diff {:d}!", _blk_height);
    }

    // get block's hash
    crypto::hash blk_hash = core_storage->get_block_id_by_height(_blk_height);

    crypto::hash prev_hash = blk.prev_id;
    crypto::hash next_hash = null_hash;

    if (_blk_height + 1 <= current_blockchain_height)
    {
        next_hash = core_storage->get_block_id_by_height(_blk_height + 1);
    }

    bool have_next_hash = (next_hash == null_hash ? false : true);
    bool have_prev_hash = (prev_hash == null_hash ? false : true);

    // remove "<" and ">" from the hash string
    string prev_hash_str = pod_to_hex(prev_hash);
    string next_hash_str = pod_to_hex(next_hash);

    // remove "<" and ">" from the hash string
    string blk_hash_str  = pod_to_hex(blk_hash);

    // get block timestamp in user friendly format
    string blk_timestamp = xmreg::timestamp_to_str_gm(blk.timestamp);

    // get age of the block relative to the server time
    pair<string, string> age = get_age(server_timestamp, blk.timestamp);

    // get time from the last block
    string delta_time {"N/A"};

    if (have_prev_hash)
    {
        block prev_blk = core_storage->get_db().get_block(prev_hash);

        pair<string, string> delta_diff = get_age(blk.timestamp, prev_blk.timestamp);

        delta_time = delta_diff.first;
    }

    // get block size in bytes
    uint64_t blk_size = core_storage->get_db().get_block_weight(_blk_height);

    // miner reward tx
    transaction coinbase_tx = blk.miner_tx;

    // transcation in the block
    vector<crypto::hash> tx_hashes = blk.tx_hashes;

    bool have_txs = !blk.tx_hashes.empty();

    // sum of all transactions in the block
    uint64_t sum_fees = 0;

    // get tx details for the coinbase tx, i.e., miners reward
    tx_details txd_coinbase = get_tx_details(blk.miner_tx, true, _blk_height, current_blockchain_height);

    // initalise page tempate map with basic info about blockchain

    string blk_pow_hash_str = pod_to_hex(get_block_longhash(core_storage, blk, _blk_height, 0));
    uint64_t blk_difficulty = core_storage->get_db().get_block_difficulty(_blk_height);

    mstch::map context {
            {"testnet"              , testnet},
            {"stagenet"             , stagenet},
            {"blk_hash"             , blk_hash_str},
            {"blk_height"           , _blk_height},
            {"diff"                 , blk_diff},
            {"blk_timestamp"        , blk_timestamp},
            {"blk_timestamp_epoch"  , blk.timestamp},
            {"prev_hash"            , prev_hash_str},
            {"next_hash"            , next_hash_str},
            {"enable_as_hex"        , enable_as_hex},
            {"have_next_hash"       , have_next_hash},
            {"have_prev_hash"       , have_prev_hash},
            {"have_txs"             , have_txs},
            {"no_txs"               , std::to_string(blk.tx_hashes.size())},
            {"blk_age"              , age.first},
            {"delta_time"           , delta_time},
            {"blk_nonce"            , blk.nonce},
            {"blk_pow_hash"         , blk_pow_hash_str},
            {"blk_difficulty"       , blk_difficulty},
            {"age_format"           , age.second},
            {"major_ver"            , std::to_string(blk.major_version)},
            {"minor_ver"            , std::to_string(blk.minor_version)},
            {"blk_size"             , fmt::format("{:0.4f}", static_cast<double>(blk_size) / 1024.0)},
    };
    add_tx_metadata(context, blk.miner_tx, true);
    context.emplace("coinbase_txs", mstch::array{{txd_coinbase.get_mstch_map()}});
    context.emplace("blk_txs"     , mstch::array());

    // .push_back(txd_coinbase.get_mstch_map()

    // boost::get<mstch::array>(context["blk_txs"]).push_back(txd_coinbase.get_mstch_map());

    // now process nomral transactions
    // get reference to blocks template map to be field below
    mstch::array &txs = boost::get<mstch::array>(context["blk_txs"]);

    // timescale representation for each tx in the block
    vector<string> mixin_timescales_str;

    // for each transaction in the block
    for (size_t i = 0; i < blk.tx_hashes.size(); ++i)
    {
        // get transaction info of the tx in the mempool
        const crypto::hash &tx_hash = blk.tx_hashes.at(i);

        // remove "<" and ">" from the hash string
        string tx_hash_str = pod_to_hex(tx_hash);


        // get transaction
        transaction tx;

        if (!mcore->get_tx(tx_hash, tx))
        {
            cerr << "Cant get tx: " << tx_hash << endl;
            continue;
        }

        tx_details txd = get_tx_details(tx, false, _blk_height, current_blockchain_height);

        // add fee to the rest
        sum_fees += txd.fee;


        // get mixins in time scale for visual representation
        //string mixin_times_scale = xmreg::timestamps_time_scale(mixin_timestamps,
        //                                                        server_timestamp);


        // add tx details mstch map to context
        txs.push_back(txd.get_mstch_map());
    }


    // add total fees in the block to the context
    context["sum_fees"] = xmreg::arq_amount_to_str(sum_fees, "{:0.9f}", false);

    // get arq in the block reward
    context["blk_reward"] = xmreg::arq_amount_to_str(txd_coinbase.arq_outputs - sum_fees, "{:0.9f}");

    add_css_style(context);

    // render the page
    return mstch::render(template_file["block"], context);
}


string
show_block(string _blk_hash)
{
    crypto::hash blk_hash;

    if (!xmreg::parse_str_secret_key(_blk_hash, blk_hash))
    {
        cerr << "Cant parse blk hash: " << blk_hash << endl;
        return fmt::format("Cant get block {:s} due to block hash parse error!", blk_hash);
    }

    uint64_t blk_height;

    if (core_storage->have_block(blk_hash))
    {
        blk_height = core_storage->get_db().get_block_height(blk_hash);
    }
    else
    {
        cerr << "Cant get block: " << blk_hash << endl;
        return fmt::format("Cant get block {:s}", blk_hash);
    }

    return show_block(blk_height);
}

string
show_service_node(const std::string &service_node_pubkey)
{
  COMMAND_RPC_GET_SERVICE_NODES::response response;
  if (!rpc.get_service_node(response, {service_node_pubkey}))
  {
    cerr << "Failed to rpc with daemon " << service_node_pubkey << endl;
    return std::string("Failed to rpc with daemon " + service_node_pubkey);
  }

  if (response.service_node_states.size() != 1)
  {
    cerr << "service node state size: " << response.service_node_states.size() << endl;
    cerr << "Can't get service node pubkey or couldn't find as registered service node: " << service_node_pubkey << endl;
    return std::string("Can't get service node pubkey or couldn't find as registered service node: " + service_node_pubkey);
  }

  auto &sn = response.service_node_states[0];
  mstch::map page_context{};
  set_service_node_fields(page_context, sn);

  if (!sn.funded)
  {
    mstch::array pending_stakes;
    for (const auto &mempool_tx : MempoolStatus::get_mempool_txs())
    {
      cryptonote::account_public_address contributor;
      if (get_service_node_contributor_from_tx_extra(mempool_tx.tx.extra, contributor))
      {
        auto sn_key = extract_sn_pubkey(mempool_tx.tx.extra);
        if (sn_key != service_node_pubkey)
          continue;
        mstch::map contrib;
        contrib["txid"] = pod_to_hex(mempool_tx.tx_hash);
        contrib["address"] = get_account_address_as_str(nettype, false, contributor);
        uint64_t amount = get_amount_from_stake(mempool_tx.tx, contributor);
        contrib["amount"] = amount > 0 ? xmreg::arq_amount_to_str(amount, "{:0.9f}", true) : "<decode error>";
        pending_stakes.push_back(std::move(contrib));
      }
    }

    if (!pending_stakes.empty())
    {
      page_context["pending_stakes_size"] = pending_stakes.size();
      page_context["pending_stakes"] = std::move(pending_stakes);
    }
  }

  add_css_style(page_context);
  return mstch::render(template_file["service_node_detail"], page_context);
}

string
show_tx(string tx_hash_str, uint16_t with_ring_signatures = 0)
{

    // parse tx hash string to hash object
    crypto::hash tx_hash;

    if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
    {
        cerr << "Cant parse tx hash: " << tx_hash_str << endl;
        return string("Cant get tx hash due to parse error: " + tx_hash_str);
    }

    // tx age
    pair<string, string> age;

    string blk_timestamp {"N/A"};

    // get transaction
    transaction tx;

    bool show_more_details_link {true};

    if (!mcore->get_tx(tx_hash, tx))
    {
        cerr << "Cant get tx in blockchain: " << tx_hash
             << ". \n Check mempool now" << endl;

        vector<MempoolStatus::mempool_tx> found_txs;

        search_mempool(tx_hash, found_txs);

        if (!found_txs.empty())
        {
            // there should be only one tx found
            tx = found_txs.at(0).tx;

            // since its tx in mempool, it has no blk yet
            // so use its recive_time as timestamp to show

            uint64_t tx_recieve_timestamp = found_txs.at(0).receive_time;

            blk_timestamp = xmreg::timestamp_to_str_gm(tx_recieve_timestamp);

            age = get_age(server_timestamp, tx_recieve_timestamp, FULL_AGE_FORMAT);

            // for mempool tx, we dont show more details, e.g., json tx representation
            // so no need for the link
            // show_more_details_link = false;
        }
        else
        {
            // tx is nowhere to be found :-(
            return string("Cant get tx: " + tx_hash_str);
        }
    }

    mstch::map tx_context;

    tx_context = construct_tx_context(tx, static_cast<bool>(with_ring_signatures));

    tx_context["show_more_details_link"] = show_more_details_link;

    if (boost::get<bool>(tx_context["has_error"]))
    {
        return boost::get<string>(tx_context["error_msg"]);
    }

    mstch::map context {
            {"testnet"          , this->testnet},
            {"stagenet"         , this->stagenet},
            {"txs"              , mstch::array{}}
    };

    boost::get<mstch::array>(context["txs"]).push_back(tx_context);

    map<string, string> partials
    {
      {"tx_details", template_file["tx_details"]},
    };

    add_css_style(context);

    // render the page
    return mstch::render(template_file["tx"], context, partials);
}

string
show_tx_hex(string tx_hash_str)
{
    transaction tx;
    crypto::hash tx_hash;

    if (!get_tx(tx_hash_str, tx, tx_hash))
        return string {"Cant get tx: "} +  tx_hash_str;

    try
    {
        return tx_to_hex(tx);
    }
    catch (std::exception const& e)
    {
        cerr << e.what() << endl;
        return string {"Failed to obtain hex of tx due to: "} + e.what();
    }
}

string
show_block_hex(size_t block_height, bool complete_blk)
{

    // get transaction
    block blk;

    if (!mcore->get_block_by_height(block_height, blk))
    {
        cerr << "Cant get block in blockchain: " << block_height
             << ". \n Check mempool now\n";
    }

    try
    {
        if (complete_blk == false)
        {
            // get only block data as hex

            return epee::string_tools::buff_to_hex_nodelimer(
                        t_serializable_object_to_blob(blk));
        }
        else
        {
            // get block_complete_entry (block and its txs) as hex

            block_complete_entry complete_block_data;

            if (!mcore->get_block_complete_entry(blk, complete_block_data))
            {
                cerr << "Failed to obtain complete block data " << endl;
                return string {"Failed to obtain complete block data "};
            }

            epee::byte_slice complete_block_data_slice;

            if(!epee::serialization::store_t_to_binary(
                        complete_block_data, complete_block_data_slice))
            {
                cerr << "Failed to serialize complete_block_data\n";
                return string {"Failed to obtain complete block data"};
            }

            std::string block_data_str(
              complete_block_data_slice.begin(),
              complete_block_data_slice.end());

            return epee::string_tools::buff_to_hex_nodelimer(block_data_str);
        }
    }
    catch (std::exception const& e)
    {
        cerr << e.what() << endl;
        return string {"Failed to obtain hex of a block due to: "} + e.what();
    }
}

string
show_ringmembers_hex(string const& tx_hash_str)
{
    transaction tx;
    crypto::hash tx_hash;

    if (!get_tx(tx_hash_str, tx, tx_hash))
        return string {"Cant get tx: "} +  tx_hash_str;

    vector<txin_to_key> input_key_imgs = xmreg::get_key_images(tx);

    // key: vector of absolute_offsets and associated amount (last value),
    // value: vector of output_info_of_mixins
    std::map<vector<uint64_t>, vector<string>> all_mixin_outputs;

       // make timescale maps for mixins in input
    for (txin_to_key const &in_key: input_key_imgs)
    {
        // get absolute offsets of mixins
        std::vector<uint64_t> absolute_offsets
                = cryptonote::relative_output_offsets_to_absolute(
                        in_key.key_offsets);

        // get public keys of outputs used in the mixins that
        // match to the offests
        std::vector<cryptonote::output_data_t> mixin_outputs;

        try
        {
            // before proceeding with geting the outputs based on
            // the amount and absolute offset
            // check how many outputs there are for that amount
            // go to next input if a too large offset was found
            if (are_absolute_offsets_good(absolute_offsets, in_key) == false)
                continue;

            core_storage->get_db().get_output_key(epee::span<const uint64_t>(&in_key.amount, 1),
                                                  absolute_offsets,
                                                  mixin_outputs);
        }
        catch (OUTPUT_DNE const &e)
        {
            cerr << "get_output_keys: " << e.what() << endl;
            continue;
        }

        // add accociated amount to these offsets so that we can differentiate
        // between same offsets, but for different amounts
        absolute_offsets.push_back(in_key.amount);

        for (auto const &mo: mixin_outputs)
            all_mixin_outputs[absolute_offsets].emplace_back(pod_to_hex(mo));

    } // for (txin_to_key const& in_key: input_key_imgs)

    if (all_mixin_outputs.empty())
        return string {"No ring members to serialize"};

    // archive all_mixin_outputs vector
    std::ostringstream oss;
    boost::archive::portable_binary_oarchive archive(oss);
    archive << all_mixin_outputs;

    // return as all_mixin_outputs vector hex
    return epee::string_tools::buff_to_hex_nodelimer(oss.str());
}

string
show_ringmemberstx_hex(string const &tx_hash_str)
{
    transaction tx;
    crypto::hash tx_hash;

    if (!get_tx(tx_hash_str, tx, tx_hash))
        return string {"Cant get tx: "} + tx_hash_str;

    vector<txin_to_key> input_key_imgs = xmreg::get_key_images(tx);

    // key: constracted from concatenation of in_key.amount and absolute_offsets,
    // value: vector of string where string is transaction hash + output index + tx_hex
    // will have to cut this string when de-seraializing this data
    // later in the unit tests
    // transaction hash and output index represent tx_out_index
    std::map<string, vector<string>> all_mixin_txs;

    for (txin_to_key const &in_key: input_key_imgs)
    {
        // get absolute offsets of mixins
        std::vector<uint64_t> absolute_offsets
                = cryptonote::relative_output_offsets_to_absolute(
                        in_key.key_offsets);

        //tx_out_index is pair::<transaction hash, output index>
        vector<tx_out_index> indices;

        // get tx hashes and indices in the txs for the
        // given outputs of mixins
        //  this cant THROW DB_EXCEPTION
        try
        {
            // get tx of the real output
            core_storage->get_db().get_output_tx_and_index(
                        in_key.amount, absolute_offsets, indices);
        }
        catch (exception const &e)
        {

            string out_msg = fmt::format(
                    "Cant get ring member tx_out_index for tx {:s}", tx_hash_str
            );

            cerr << out_msg << endl;

            return string(out_msg);
        }

        string map_key = std::to_string(in_key.amount);

        for (auto const &ao: absolute_offsets)
            map_key += std::to_string(ao);

        // serialize each mixin tx
        for (auto const &txi : indices)
        {
           auto const &mixin_tx_hash = txi.first;
           auto const &output_index_in_tx = txi.second;

           transaction mixin_tx;

           if (!mcore->get_tx(mixin_tx_hash, mixin_tx))
           {
               throw std::runtime_error("Cant get tx: "
                                        + pod_to_hex(mixin_tx_hash));
           }

           // serialize tx
           string tx_hex = epee::string_tools::buff_to_hex_nodelimer(
                                   t_serializable_object_to_blob(mixin_tx));

           all_mixin_txs[map_key].push_back(
                       pod_to_hex(mixin_tx_hash)
                       + std::to_string(output_index_in_tx)
                       + tx_hex);
        }

    } // for (txin_to_key const& in_key: input_key_imgs)

    if (all_mixin_txs.empty())
        return string {"No ring members to serialize"};

    // archive all_mixin_outputs vector
    std::ostringstream oss;
    boost::archive::portable_binary_oarchive archive(oss);
    archive << all_mixin_txs;

    // return as all_mixin_outputs vector hex
    return epee::string_tools::buff_to_hex_nodelimer(oss.str());
}

/**
 * @brief Get ring member tx data
 *
 * Used for generating json file of txs used in unit testing.
 * Thanks to json output from this function, we can mock
 * a number of blockchain quries about key images
 *
 * @param tx_hash_str
 * @return
 */
json
show_ringmemberstx_jsonhex(string const &tx_hash_str)
{
    transaction tx;
    crypto::hash tx_hash;

    if (!get_tx(tx_hash_str, tx, tx_hash))
        return string {"Cant get tx: "} + tx_hash_str;

    vector<txin_to_key> input_key_imgs = xmreg::get_key_images(tx);

    json tx_json;

    string tx_hex;

    try
    {
        tx_hex = tx_to_hex(tx);
    }
    catch (std::exception const &e)
    {
        cerr << e.what() << endl;
        return json {"error", "Failed to obtain hex of tx"};
    }

    tx_json["hash"] = tx_hash_str;
    tx_json["hex"]  = tx_hex;
    tx_json["nettype"] = static_cast<size_t>(nettype);
    tx_json["is_ringct"] = tx.version >= cryptonote::txversion::v2;
    tx_json["rct_type"] = tx.rct_signatures.type;

    tx_json["_comment"] = "Just a placeholder for some comment if needed later";

    // add placeholder for sender and recipient details
    // this is most useful for unit testing on stagenet/testnet
    // private monero networks, so we can easly put these
    // networks accounts details here.
    tx_json["sender"] = json {
                            {"seed", ""},
                            {"address", ""},
                            {"viewkey", ""},
                            {"spendkey", ""},
                            {"amount", 0ull},
                            {"change", 0ull},
                            {"outputs", json::array({json::array(
                                                    {"index placeholder",
                                                     "public_key placeholder",
                                                     "amount placeholder"}
                                        )})
                            },
                            {"_comment", ""}};

    tx_json["recipient"] = json::array();

    tx_json["recipient"].push_back(
                              json { {"seed", ""},
                                   {"address", ""},
                                   {"is_subaddress", false},
                                   {"viewkey", ""},
                                   {"spendkey", ""},
                                   {"amount", 0ull},
                                   {"outputs", json::array({json::array(
                                                           {"index placeholder",
                                                            "public_key placeholder",
                                                            "amount placeholder"}
                                               )})
                                   },
                                   {"_comment", ""}});

    uint64_t tx_blk_height {0};

    try
    {
        tx_blk_height = core_storage->get_db().get_tx_block_height(tx_hash);
    }
    catch (exception& e)
    {
        cerr << "Cant get block height: " << tx_hash
             << e.what() << endl;

        return json {"error", "Cant get block height"};
    }

    // get block cointaining this tx
    block blk;

    if ( !mcore->get_block_by_height(tx_blk_height, blk))
    {
        cerr << "Cant get block: " << tx_blk_height << endl;
        return json {"error", "Cant get block"};
    }

    block_complete_entry complete_block_data;

    if (!mcore->get_block_complete_entry(blk, complete_block_data))
    {
        cerr << "Failed to obtain complete block data " << endl;
        return json {"error", "Failed to obtain complete block data "};
    }

    epee::byte_slice complete_block_data_slice;

    if(!epee::serialization::store_t_to_binary(
                complete_block_data, complete_block_data_slice))
    {
        cerr << "Failed to serialize complete_block_data\n";
        return json {"error", "Failed to obtain complete block data"};
    }

    tx_details txd = get_tx_details(tx);

    tx_json["payment_id"] = pod_to_hex(txd.payment_id);
    tx_json["payment_id8"] = pod_to_hex(txd.payment_id8);
    tx_json["payment_id8e"] = pod_to_hex(txd.payment_id8);

    std::string complete_block_data_str(complete_block_data_slice.begin(), complete_block_data_slice.end());

    tx_json["block"] = epee::string_tools::buff_to_hex_nodelimer(complete_block_data_str);

    tx_json["block_version"] = json {blk.major_version, blk.minor_version};

    tx_json["inputs"] = json::array();


    // key: constracted from concatenation of in_key.amount and absolute_offsets,
    // value: vector of string where string is transaction hash + output index + tx_hex
    // will have to cut this string when de-seraializing this data
    // later in the unit tests
    // transaction hash and output index represent tx_out_index
    std::map<string, vector<string>> all_mixin_txs;

    for (txin_to_key const &in_key: input_key_imgs)
    {
        // get absolute offsets of mixins
        std::vector<uint64_t> absolute_offsets
                = cryptonote::relative_output_offsets_to_absolute(
                        in_key.key_offsets);

        //tx_out_index is pair::<transaction hash, output index>
        vector<tx_out_index> indices;
        std::vector<output_data_t> mixin_outputs;

        // get tx hashes and indices in the txs for the
        // given outputs of mixins
        //  this cant THROW DB_EXCEPTION
        try
        {
            // get tx of the real output
            core_storage->get_db().get_output_tx_and_index(
                        in_key.amount, absolute_offsets, indices);

            // get mining ouput info
            core_storage->get_db().get_output_key(
                        epee::span<const uint64_t>(&in_key.amount, 1),
                        absolute_offsets,
                        mixin_outputs);
        }
        catch (exception const &e)
        {

            string out_msg = fmt::format(
                    "Cant get ring member tx_out_index for tx {:s}", tx_hash_str
            );

            cerr << out_msg << endl;

            return json {"error", out_msg};
        }


        tx_json["inputs"].push_back(json {{"key_image", pod_to_hex(in_key.k_image)},
                                          {"amount", in_key.amount},
                                          {"absolute_offsets", absolute_offsets},
                                          {"ring_members", json::array()}});

        json& ring_members = tx_json["inputs"].back()["ring_members"];


        if (indices.size() != mixin_outputs.size())
        {
            cerr << "indices.size() != mixin_outputs.size()\n";
            return json {"error", "indices.size() != mixin_outputs.size()"};
        }

        // serialize each mixin tx
        //for (auto const& txi : indices)
        for (size_t i = 0; i < indices.size(); ++i)
        {

           tx_out_index const &txi = indices[i];
           output_data_t const &mo = mixin_outputs[i];

           auto const& mixin_tx_hash = txi.first;
           auto const& output_index_in_tx = txi.second;

           transaction mixin_tx;

           if (!mcore->get_tx(mixin_tx_hash, mixin_tx))
           {
               throw std::runtime_error("Cant get tx: "
                                        + pod_to_hex(mixin_tx_hash));
           }

           // serialize tx
           string tx_hex = epee::string_tools::buff_to_hex_nodelimer(
                                   t_serializable_object_to_blob(mixin_tx));

           ring_members.push_back(
                   json {
                          {"ouput_pk", pod_to_hex(mo.pubkey)},
                          {"tx_hash", pod_to_hex(mixin_tx_hash)},
                          {"output_index_in_tx", txi.second},
                          {"tx_hex", tx_hex},
                   });

        }

    } // for (txin_to_key const& in_key: input_key_imgs)


    // archive all_mixin_outputs vector
    std::ostringstream oss;
    boost::archive::portable_binary_oarchive archive(oss);
    archive << all_mixin_txs;

    // return as all_mixin_outputs vector hex
    //return epee::string_tools
    //        ::buff_to_hex_nodelimer(oss.str());

    return tx_json;
}



string
show_my_outputs(string tx_hash_str,
                string arq_address_str,
                string viewkey_str, /* or tx_prv_key_str when tx_prove == true */
                string raw_tx_data,
                string domain,
                bool tx_prove = false)
{

    // remove white characters
    boost::trim(tx_hash_str);
    boost::trim(arq_address_str);
    boost::trim(viewkey_str);
    boost::trim(raw_tx_data);

    if (tx_hash_str.empty())
    {
        return string("tx hash not provided!");
    }

    if (arq_address_str.empty())
    {
        return string("Arqma address not provided!");
    }

    if (viewkey_str.empty())
    {
        if (!tx_prove)
            return string("Viewkey not provided!");
        else
            return string("Tx private key not provided!");
    }

    // parse tx hash string to hash object
    crypto::hash tx_hash;

    if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
    {
        cerr << "Cant parse tx hash: " << tx_hash_str << endl;
        return string("Cant get tx hash due to parse error: " + tx_hash_str);
    }

    // parse string representing given monero address
    cryptonote::address_parse_info address_info;

    if (!xmreg::parse_str_address(arq_address_str, address_info, nettype))
    {
        cerr << "Cant parse string address: " << arq_address_str << endl;
        return string("Cant parse arq address: " + arq_address_str);
    }

    // parse string representing given private key
    crypto::secret_key prv_view_key;

    std::vector<crypto::secret_key> multiple_tx_secret_keys;

    if (!xmreg::parse_str_secret_key(viewkey_str, multiple_tx_secret_keys))
    {
        cerr << "Cant parse the private key: " << viewkey_str << endl;
        return string("Cant parse private key: " + viewkey_str);
    }
    if (multiple_tx_secret_keys.size() == 1)
    {
        prv_view_key = multiple_tx_secret_keys[0];
    }
    else if (!tx_prove)
    {
        cerr << "Concatenated secret keys are only for tx proving!" << endl;
        return string("Concatenated secret keys are only for tx proving!");
    }


    // just to see how would having spend keys could worked
    // this is from testnet wallet: A2VTvE8bC9APsWFn3mQzgW8Xfcy2SP2CRUArD6ZtthNaWDuuvyhtBcZ8WDuYMRt1HhcnNQvpXVUavEiZ9waTbyBhP6RM8TV
    // view key: 041a241325326f9d86519b714a9b7f78b29111551757eeb6334d39c21f8b7400
    // example tx: 430b070e213659a864ec82d674fddb5ccf7073cae231b019ba1ebb4bfdc07a15
//        string spend_key_str("643fedcb8dca1f3b406b84575ecfa94ba01257d56f20d55e8535385503dacc08");
//
//        crypto::secret_key prv_spend_key;
//        if (!xmreg::parse_str_secret_key(spend_key_str, prv_spend_key))
//        {
//            cerr << "Cant parse the prv_spend_key : " << spend_key_str << endl;
//            return string("Cant parse prv_spend_key : " + spend_key_str);
//        }

    // tx age
    pair<string, string> age;

    string blk_timestamp {"N/A"};

    // get transaction
    transaction tx;

    if (!raw_tx_data.empty())
    {
        // we want to check outputs of tx submited through tx pusher.
        // it is raw tx data, it is not in blockchain nor in mempool.
        // so we need to reconstruct tx object from this string

        cryptonote::blobdata tx_data_blob;

        if (!epee::string_tools::parse_hexstr_to_binbuff(raw_tx_data, tx_data_blob))
        {
            string msg = fmt::format("Cant obtain tx_data_blob from raw_tx_data");

            cerr << msg << endl;

            return msg;
        }

        crypto::hash tx_hash_from_blob;
        crypto::hash tx_prefix_hash_from_blob;

        if (!cryptonote::parse_and_validate_tx_from_blob(tx_data_blob,
                                                         tx,
                                                         tx_hash_from_blob,
                                                         tx_prefix_hash_from_blob))
        {
            string msg = fmt::format("cant parse_and_validate_tx_from_blob");

            cerr << msg << endl;

            return msg;
        }

    }
    else if (!mcore->get_tx(tx_hash, tx))
    {
        cerr << "Cant get tx in blockchain: " << tx_hash
             << ". \n Check mempool now" << endl;

        vector<MempoolStatus::mempool_tx> found_txs;

        search_mempool(tx_hash, found_txs);

        if (!found_txs.empty())
        {
            // there should be only one tx found
            tx = found_txs.at(0).tx;

            // since its tx in mempool, it has no blk yet
            // so use its recive_time as timestamp to show

            uint64_t tx_recieve_timestamp
                    = found_txs.at(0).receive_time;

            blk_timestamp = xmreg::timestamp_to_str_gm(tx_recieve_timestamp);

            age = get_age(server_timestamp,
                          tx_recieve_timestamp,
                          FULL_AGE_FORMAT);
        }
        else
        {
            // tx is nowhere to be found :-(
            return string("Cant get tx: " + tx_hash_str);
        }
    }

    tx_details txd = get_tx_details(tx);

    uint64_t tx_blk_height {0};

    bool tx_blk_found {false};

    try
    {
        tx_blk_height = core_storage->get_db().get_tx_block_height(tx_hash);
        tx_blk_found = true;
    }
    catch (exception& e)
    {
        cerr << "Cant get block height: " << tx_hash
             << e.what() << endl;
    }

    // get block cointaining this tx
    block blk;

    if (tx_blk_found && !mcore->get_block_by_height(tx_blk_height, blk))
    {
        cerr << "Cant get block: " << tx_blk_height << endl;
    }

    string tx_blk_height_str {"N/A"};

    if (tx_blk_found)
    {
        // calculate difference between tx and server timestamps
        age = get_age(server_timestamp, blk.timestamp, FULL_AGE_FORMAT);

        blk_timestamp = xmreg::timestamp_to_str_gm(blk.timestamp);

        tx_blk_height_str = std::to_string(tx_blk_height);
    }

    // payments id. both normal and encrypted (payment_id8)
    string pid_str   = pod_to_hex(txd.payment_id);
    string pid8_str  = pod_to_hex(txd.payment_id8);

    string shortcut_url = tx_prove ? string("/prove") : string("/myoutputs")
                          + '/' + tx_hash_str
                          + '/' + arq_address_str
                          + '/' + viewkey_str;


    string viewkey_str_partial = viewkey_str;

    // dont show full private keys. Only file first and last letters
    for (size_t i = 3; i < viewkey_str_partial.length() - 2; ++i)
        viewkey_str_partial[i] = '*';

    // initalise page tempate map with basic info about blockchain
    mstch::map context {
            {"testnet"              , testnet},
            {"stagenet"             , stagenet},
            {"tx_hash"              , tx_hash_str},
            {"tx_prefix_hash"       , pod_to_hex(txd.prefix_hash)},
            {"arq_address"          , arq_address_str},
            {"viewkey"              , viewkey_str_partial},
            {"tx_pub_key"           , pod_to_hex(txd.pk)},
            {"blk_height"           , tx_blk_height_str},
            {"tx_size"              , fmt::format("{:0.4f}", static_cast<double>(txd.size) / 1024.0)},
            {"tx_fee"               , xmreg::arq_amount_to_str(txd.fee, "{:0.9f}", true)},
            {"blk_timestamp"        , blk_timestamp},
            {"delta_time"           , age.first},
            {"outputs_no"           , static_cast<uint64_t>(txd.output_pub_keys.size())},
            {"has_payment_id"       , txd.payment_id  != null_hash},
            {"has_payment_id8"      , txd.payment_id8 != null_hash8},
            {"payment_id"           , pid_str},
            {"payment_id8"          , pid8_str},
            {"decrypted_payment_id8", string{}},
            {"tx_prove"             , tx_prove},
            {"domain_url"           , domain},
            {"shortcut_url"         , shortcut_url}
    };

    string server_time_str = xmreg::timestamp_to_str_gm(server_timestamp, "%F");



    // public transaction key is combined with our viewkey
    // to create, so called, derived key.
    key_derivation derivation;
    std::vector<key_derivation> additional_derivations(txd.additional_pks.size());

    if (tx_prove && multiple_tx_secret_keys.size()
            != txd.additional_pks.size() + 1)
    {
        return string("This transaction includes additional tx pubkeys whose "
                       "size doesn't match with the provided tx secret keys");
    }

    public_key pub_key = tx_prove ? address_info.address.m_view_public_key : txd.pk;

    //cout << "txd.pk: " << pod_to_hex(txd.pk) << endl;

    if (!generate_key_derivation(pub_key,
                                 tx_prove ? multiple_tx_secret_keys[0] : prv_view_key,
                                 derivation))
    {
        cerr << "Cant get derived key for: "  << "\n"
             << "pub_tx_key: " << pub_key << " and "
             << "prv_view_key" << pod_to_hex(unwrap(unwrap(prv_view_key))) << endl;

        return string("Cant get key_derivation");
    }

    for (size_t i = 0; i < txd.additional_pks.size(); ++i)
    {
        if (!generate_key_derivation(tx_prove ? pub_key : txd.additional_pks[i],
                                     tx_prove ? multiple_tx_secret_keys[i + 1] : prv_view_key,
                                     additional_derivations[i]))
        {
            cerr << "Cant get derived key for: "  << "\n"
                 << "pub_tx_key: " << txd.additional_pks[i] << " and "
                 << "prv_view_key" << pod_to_hex(unwrap(unwrap(prv_view_key))) << endl;

            return string("Cant get key_derivation");
        }
    }

    // decrypt encrypted payment id, as used in integreated addresses
    crypto::hash8 decrypted_payment_id8 = txd.payment_id8;

    if (decrypted_payment_id8 != null_hash8)
    {
        if (mcore->get_device()->decrypt_payment_id(
                    decrypted_payment_id8, pub_key, prv_view_key))
        {
            context["decrypted_payment_id8"] = pod_to_hex(decrypted_payment_id8);
        }
    }

    mstch::array outputs;

    uint64_t sum_arq {0};

    std::vector<uint64_t> money_transfered(tx.vout.size(), 0);

    //std::deque<rct::key> mask(tx.vout.size());

    uint64_t output_idx {0};

    for (pair<txout_to_key, uint64_t> &outp: txd.output_pub_keys)
    {

        // get the tx output public key
        // that normally would be generated for us,
        // if someone had sent us some xmr.
        public_key tx_pubkey;

        derive_public_key(derivation,
                          output_idx,
                          address_info.address.m_spend_public_key,
                          tx_pubkey);

//        cout << pod_to_hex(derivation) << ", " << output_idx << ", "
//             << pod_to_hex(address_info.address.m_spend_public_key) << ", "
//             << pod_to_hex(outp.first.key) << " == "
//             << pod_to_hex(tx_pubkey) << '\n'  << '\n';

        // check if generated public key matches the current output's key
        bool mine_output = (outp.first.key == tx_pubkey);

        bool with_additional = false;

        if (!mine_output && txd.additional_pks.size() == txd.output_pub_keys.size())
        {
            derive_public_key(additional_derivations[output_idx],
                              output_idx,
                              address_info.address.m_spend_public_key,
                              tx_pubkey);


            mine_output = (outp.first.key == tx_pubkey);

            with_additional = true;
        }

        // if mine output has RingCT, i.e., tx version is 2
        if (mine_output && tx.version >= cryptonote::txversion::v2)
        {
            // cointbase txs have amounts in plain sight.
            // so use amount from ringct, only for non-coinbase txs
            if (!is_coinbase(tx))
            {

                // initialize with regular amount
                uint64_t rct_amount = money_transfered[output_idx];

                bool r;

                r = decode_ringct(
                            tx.rct_signatures,
                            with_additional
                              ? additional_derivations[output_idx] : derivation,
                            output_idx,
                            tx.rct_signatures.ecdhInfo[output_idx].mask,
                            rct_amount);

                if (!r)
                {
                    cerr << "\nshow_my_outputs: Cant decode RingCT!\n";
                }

                outp.second         = rct_amount;
                money_transfered[output_idx] = rct_amount;
            }

        }

        if (mine_output)
        {
            sum_arq += outp.second;
        }

        outputs.push_back(mstch::map {
                {"out_pub_key"           , pod_to_hex(outp.first.key)},
                {"amount"                , xmreg::arq_amount_to_str(outp.second)},
                {"mine_output"           , mine_output},
                {"output_idx"            , fmt::format("{:02d}", output_idx)}
        });

        ++output_idx;
    }

    context.emplace("outputs", outputs);

    context["found_our_outputs"] = (sum_arq > 0);
    context["sum_arq"] = xmreg::arq_amount_to_str(sum_arq);

    // we can also test ouputs used in mixins for key images
    // this can show possible spending. Only possible, because
    // without a spend key, we cant know for sure. It might be
    // that our output was used by someone else for their mixins.

    if (enable_mixin_guess) {

      bool show_key_images {false};

      mstch::array inputs;

      vector<txin_to_key> input_key_imgs = xmreg::get_key_images(tx);

      // to hold sum of xmr in matched mixins, those that
      // perfectly match mixin public key with outputs in mixn_tx.
      uint64_t sum_mixin_arq {0};

      // this is used for the final check. we assument that number of
      // parefct matches must be equal to number of inputs in a tx.
      uint64_t no_of_matched_mixins {0};

      // Hold all possible mixins that we found. This is only used so that
      // we get number of all posibilities, and their total xmr amount
      // (useful for unit testing)
      //                     public_key    , amount
      std::vector<std::pair<crypto::public_key, uint64_t>> all_possible_mixins;

      for (const txin_to_key& in_key: input_key_imgs)
      {
        std::vector<uint64_t> absolute_offsets = cryptonote::relative_output_offsets_to_absolute(in_key.key_offsets);
        std::vector<cryptonote::output_data_t> mixin_outputs;
        try
        {
          if (are_absolute_offsets_good(absolute_offsets, in_key) == false)
            continue;

          core_storage->get_db().get_output_key(epee::span<const uint64_t>(&in_key.amount, 1), absolute_offsets, mixin_outputs);
        }
        catch (const OUTPUT_DNE& e)
        {
          cerr << "get_output_keys: " << e.what() << endl;
          continue;
        }

        inputs.push_back(mstch::map{
          {"key_image", pod_to_hex(in_key.k_image)},
          {"key_image_amount", xmreg::arq_amount_to_str(in_key.amount)},
          make_pair(string("mixins"), mstch::array{})
        });

        mstch::array& mixins = boost::get<mstch::array>(boost::get<mstch::map>(inputs.back())["mixins"]);

        vector<map<string, string>> our_mixins_found;

        size_t count = 0;
        size_t no_of_output_matches_found {0};

        for (const uint64_t& abs_offset : absolute_offsets)
        {
          cryptonote::output_data_t output_data = mixin_outputs.at(count);

          tx_out_index tx_out_idx;
          try
          {
            tx_out_idx = core_storage->get_db().get_output_tx_and_index(in_key.amount, abs_offset);
          }
          catch (const OUTPUT_DNE& e)
          {
            string out_msg = fmt::format("Output with amount {:d} and index {:d} does not exist!", in_key.amount, abs_offset);
            cerr << out_msg << '\n';
            break;
          }

          string out_pub_key_str = pod_to_hex(output_data.pubkey);

          transaction mixin_tx;

          if (!mcore->get_tx(tx_out_idx.first, mixin_tx))
          {
            cerr << "Can't get tx: " << tx_out_idx.first << endl;
            break;
          }

          string mixin_tx_hash_str = pod_to_hex(tx_out_idx.first);

          mixins.push_back(mstch::map{
            {"mixin_pub_key"      , out_pub_key_str},
            make_pair<string, mstch::array>("mixin_outputs", mstch::array{}),
            {"has_mixin_outputs"  , false}});

          mstch::array& mixin_outputs = boost::get<mstch::array>(boost::get<mstch::map>(mixins.back())["mixin_outputs"]);
          mstch::node& has_mixin_outputs = boost::get<mstch::map>(mixins.back())["has_mixin_outputs"];
          bool found_something {false};

          public_key mixin_tx_pub_key = xmreg::get_tx_pub_key_from_received_outs(mixin_tx);
          std::vector<public_key> mixin_additional_tx_pub_keys = cryptonote::get_additional_tx_pub_keys_from_extra(mixin_tx);
          string mixin_tx_pub_key_str = pod_to_hex(mixin_tx_pub_key);
          key_derivation derivation;

          std::vector<key_derivation> additional_derivations(mixin_additional_tx_pub_keys.size());
          if (!generate_key_derivation(mixin_tx_pub_key, prv_view_key, derivation))
          {
            cerr << "Cant get derived key for: " << "\n"
                 << "pub_tx_key: " << mixin_tx_pub_key << " and "
                 << "prv_view_key" << pod_to_hex(unwrap(unwrap(prv_view_key))) << endl;
            continue;
          }

          for (size_t i = 0; i < mixin_additional_tx_pub_keys.size(); ++i)
          {
            if (!generate_key_derivation(mixin_additional_tx_pub_keys[i], prv_view_key, additional_derivations[i]))
            {
              cerr << "Cant get derived key for: "  << "\n"
                   << "pub_tx_key: " << mixin_additional_tx_pub_keys[i]
                   << " and prv_view_key" << pod_to_hex(unwrap(unwrap(prv_view_key))) << endl;
              continue;
            }
          }
          //          <public_key  , amount  , out idx>
          vector<tuple<txout_to_key, uint64_t, uint64_t>> output_pub_keys;

          output_pub_keys = xmreg::get_ouputs_tuple(mixin_tx);

          mixin_outputs.push_back(mstch::map{
            {"mix_tx_hash"      , mixin_tx_hash_str},
            {"mix_tx_pub_key"   , mixin_tx_pub_key_str},
            make_pair<string, mstch::array>("found_outputs" , mstch::array{}),
            {"has_found_outputs", false}
          });

          mstch::array &found_outputs = boost::get<mstch::array>(boost::get<mstch::map>(mixin_outputs.back())["found_outputs"]);
          mstch::node &has_found_outputs = boost::get<mstch::map>(mixin_outputs.back())["has_found_outputs"];

          uint64_t ringct_amount {0};

          // for each output in mixin tx, find the one from key_image
          // and check if its ours.
          for (const auto &mix_out: output_pub_keys)
          {
            txout_to_key const &txout_k = std::get<0>(mix_out);
            uint64_t amount           = std::get<1>(mix_out);
            uint64_t output_idx_in_tx = std::get<2>(mix_out);
            public_key tx_pubkey_generated;

            derive_public_key(derivation,
                              output_idx_in_tx,
                              address_info.address.m_spend_public_key,
                              tx_pubkey_generated);

            // check if generated public key matches the current output's key
            bool mine_output = (txout_k.key == tx_pubkey_generated);

            bool with_additional = false;

            if (!mine_output && mixin_additional_tx_pub_keys.size()
                    == output_pub_keys.size())
            {
                    derive_public_key(additional_derivations[output_idx_in_tx],
                                      output_idx_in_tx,
                                      address_info.address.m_spend_public_key,
                                      tx_pubkey_generated);

                    mine_output = (txout_k.key == tx_pubkey_generated);

                    with_additional = true;
            }


            if (mine_output && mixin_tx.version >= cryptonote::txversion::v2)
            {
                    // cointbase txs have amounts in plain sight.
                    // so use amount from ringct, only for non-coinbase txs
              if (!is_coinbase(mixin_tx))
              {
                        // initialize with regular amount
                        uint64_t rct_amount = amount;

                        bool r;

                        r = decode_ringct(
                                    mixin_tx.rct_signatures,
                                    with_additional
                                    ? additional_derivations[output_idx_in_tx] : derivation,
                                    output_idx_in_tx,
                                    mixin_tx.rct_signatures.ecdhInfo[output_idx_in_tx].mask,
                                    rct_amount);

                        if (!r)
                            cerr << "show_my_outputs: key images: "
                                    "Cant decode RingCT!\n";

                        amount = rct_amount;

              } // if (mine_output && mixin_tx.version == 2)
            }

                // makre only
            bool output_match = (txout_k.key == output_data.pubkey);

                // mark only first output_match as the "real" one
                // due to luck of better method of gussing which output
                // is real if two are found in a single input.
            output_match = output_match && no_of_output_matches_found == 0;

                // save our mixnin's public keys
            found_outputs.push_back(mstch::map {
                        {"my_public_key"   , pod_to_hex(txout_k.key)},
                        {"tx_hash"         , tx_hash_str},
                        {"mine_output"     , mine_output},
                        {"out_idx"         , output_idx_in_tx},
                        {"formed_output_pk", out_pub_key_str},
                        {"out_in_match"    , output_match},
                        {"amount"          , xmreg::arq_amount_to_str(amount)}
            });

                //cout << "txout_k.key == output_data.pubkey" << endl;

            if (mine_output)
            {
                    found_something = true;
                    show_key_images = true;

                    // increase sum_mixin_xmr only when
                    // public key of an outputs used in ring signature,
                    // matches a public key in a mixin_tx
                    if (txout_k.key != output_data.pubkey)
                        continue;

                    // sum up only first output matched found in each input
                    if (no_of_output_matches_found == 0)
                    {
                        // for regular txs, just concentrated on outputs
                        // which have same amount as the key image.
                        // for ringct its not possible to know for sure amount
                        // in key image without spend key, so we just use all
                        // for regular/old txs there must be also a match
                        // in amounts, not only in output public keys
                        if (mixin_tx.version < cryptonote::txversion::v2)
                        {
                            sum_mixin_arq += amount;
                        }
                        else
                        {
                            sum_mixin_arq += amount;
                            ringct_amount += amount;
                        }

                        no_of_matched_mixins++;
                    }


                    // generate key_image using this output
                    // just to see how would having spend keys worked
//                        crypto::key_image key_img;
//
//                        if (!xmreg::generate_key_image(derivation,
//                                                       output_idx_in_tx, /* position in the tx */
//                                                       prv_spend_key,
//                                                       address.m_spend_public_key,
//                                                       key_img)) {
//                            cerr << "Cant generate key image for output: "
//                                 << pod_to_hex(output_data.pubkey) << endl;
//                            break;
//                        }
//
//                        cout    << "output_data.pubkey: " << pod_to_hex(output_data.pubkey)
//                                << ", key_img: " << pod_to_hex(key_img)
//                                << ", key_img == input_key: " << (key_img == in_key.k_image)
//                                << endl;

                    no_of_output_matches_found++;

            } // if (mine_output)

          } // for (const pair<txout_to_key, uint64_t>& mix_out: txd.output_pub_keys)

          has_found_outputs = !found_outputs.empty();

          has_mixin_outputs = found_something;

            //   all_possible_mixins_amount += amount;

          if (found_something)
                all_possible_mixins.push_back(
                    {mixin_tx_pub_key,
                     in_key.amount == 0 ? ringct_amount : in_key.amount});

          ++count;

        } // for (const cryptonote::output_data_t& output_data: mixin_outputs)

    } //  for (const txin_to_key& in_key: input_key_imgs)


    context.emplace("inputs", inputs);

    context["show_inputs"]   = show_key_images;
    context["inputs_no"]     = static_cast<uint64_t>(inputs.size());
    context["sum_mixin_arq"] = xmreg::arq_amount_to_str(sum_mixin_arq, "{:0.9f}", false);


    uint64_t possible_spending  {0};

    //cout << "\nall_possible_mixins: " << all_possible_mixins.size() << '\n';

    // useful for unit testing as it provides total xmr sum
    // of possible mixins
    uint64_t all_possible_mixins_amount1  {0};

    for (auto &p: all_possible_mixins)
        all_possible_mixins_amount1 += p.second;

    //cout << "\all_possible_mixins_amount: " << all_possible_mixins_amount1 << '\n';

    //cout << "\nmixins: " << mix << '\n';

    context["no_all_possible_mixins"] = static_cast<uint64_t>(all_possible_mixins.size());
    context["all_possible_mixins_amount"] = all_possible_mixins_amount1;


    // show spending only if sum of mixins is more than
    // what we get + fee, and number of perferctly matched
    // mixis is equal to number of inputs
    if (sum_mixin_arq > (sum_arq + txd.fee)
        && no_of_matched_mixins == inputs.size())
    {
        //                  (outcoming    - incoming) - fee
        possible_spending = (sum_mixin_arq - sum_arq) - txd.fee;
    }

    context["possible_spending"] = xmreg::arq_amount_to_str(possible_spending, "{:0.9f}", false);

    } // if (enable_mixin_guess)

    add_css_style(context);

    // render the page
    return mstch::render(template_file["my_outputs"], context);
}

string
show_prove(string tx_hash_str,
           string arq_address_str,
           string tx_prv_key_str,
           string const &raw_tx_data,
           string domain)
{

    return show_my_outputs(tx_hash_str, arq_address_str,
                           tx_prv_key_str, raw_tx_data,
                           domain, true);
}

string
show_rawtx()
{

    // initalise page tempate map with basic info about blockchain
    mstch::map context {
            {"testnet"              , testnet},
            {"stagenet"             , stagenet}
    };

    add_css_style(context);

    // render the page
    return mstch::render(template_file["rawtx"], context);
}

string
show_checkrawtx(string raw_tx_data, string action)
{
    clean_post_data(raw_tx_data);

    string decoded_raw_tx_data = epee::string_encoding::base64_decode(raw_tx_data);

    //cout << decoded_raw_tx_data << endl;

    const size_t magiclen = strlen(UNSIGNED_TX_PREFIX);

    string data_prefix = xmreg::make_printable(decoded_raw_tx_data.substr(0, magiclen));

    bool unsigned_tx_given {false};

    if (strncmp(decoded_raw_tx_data.c_str(), UNSIGNED_TX_PREFIX, magiclen) == 0)
    {
        unsigned_tx_given = true;
    }

    // initalize page template context map
    mstch::map context {
            {"testnet"              , testnet},
            {"stagenet"             , stagenet},
            {"unsigned_tx_given"    , unsigned_tx_given},
            {"have_raw_tx"          , true},
            {"has_error"            , false},
            {"error_msg"            , string {}},
            {"data_prefix"          , data_prefix},
    };

    context.emplace("txs", mstch::array{});

    string full_page = template_file["checkrawtx"];

    add_css_style(context);

    if (unsigned_tx_given)
    {

        bool r {false};

        string s = decoded_raw_tx_data.substr(magiclen);

        ::tools::wallet2::unsigned_tx_set exported_txs;

        try
        {
            std::istringstream iss(s);
            boost::archive::portable_binary_iarchive ar(iss);
            ar >> exported_txs;

            r = true;
        }
        catch (...)
        {
            cerr << "Failed to parse unsigned tx data " << endl;
        }

        if (r)
        {
            mstch::array &txs = boost::get<mstch::array>(context["txs"]);

            for (const ::tools::wallet2::tx_construction_data &tx_cd: exported_txs.txes)
            {
                size_t no_of_sources = tx_cd.sources.size();

                const tx_destination_entry &tx_change = tx_cd.change_dts;

                crypto::hash payment_id   = null_hash;
                crypto::hash8 payment_id8 = null_hash8;

                get_payment_id(tx_cd.extra, payment_id, payment_id8);

                // payments id. both normal and encrypted (payment_id8)
                string pid_str   = REMOVE_HASH_BRACKETS(fmt::format("{:s}", payment_id));
                string pid8_str  = REMOVE_HASH_BRACKETS(fmt::format("{:s}", payment_id8));


                mstch::map tx_cd_data {
                        {"no_of_sources"      , static_cast<uint64_t>(no_of_sources)},
                        {"change_amount"      , xmreg::arq_amount_to_str(tx_change.amount)},
                        {"has_payment_id"     , (payment_id  != null_hash)},
                        {"has_payment_id8"    , (payment_id8 != null_hash8)},
                        {"payment_id"         , pid_str},
                        {"payment_id8"        , pid8_str},
                };
                tx_cd_data.emplace("dest_sources" , mstch::array{});
                tx_cd_data.emplace("dest_infos"   , mstch::array{});

                mstch::array &dest_sources = boost::get<mstch::array>(tx_cd_data["dest_sources"]);
                mstch::array &dest_infos = boost::get<mstch::array>(tx_cd_data["dest_infos"]);

                for (const tx_destination_entry &a_dest: tx_cd.splitted_dsts)
                {
                    mstch::map dest_info {
                            {"dest_address"  , get_account_address_as_str(
                                    nettype, a_dest.is_subaddress, a_dest.addr)},
                            {"dest_amount"   , xmreg::arq_amount_to_str(a_dest.amount)}
                    };

                    dest_infos.push_back(dest_info);
                }

                vector<vector<uint64_t>> mixin_timestamp_groups;
                vector<uint64_t> real_output_indices;

                uint64_t sum_outputs_amounts {0};

                for (size_t i = 0; i < no_of_sources; ++i)
                {

                    const tx_source_entry &tx_source = tx_cd.sources.at(i);

                    mstch::map single_dest_source {
                            {"output_amount"              , xmreg::arq_amount_to_str(tx_source.amount)},
                            {"real_output"                , static_cast<uint64_t>(tx_source.real_output)},
                            {"real_out_tx_key"            , pod_to_hex(tx_source.real_out_tx_key)},
                            {"real_output_in_tx_index"    , static_cast<uint64_t>(tx_source.real_output_in_tx_index)},
                    };
                    single_dest_source.emplace("outputs", mstch::array{});

                    sum_outputs_amounts += tx_source.amount;

                    //cout << "tx_source.real_output: "             << tx_source.real_output << endl;
                    //cout << "tx_source.real_out_tx_key: "         << tx_source.real_out_tx_key << endl;
                    //cout << "tx_source.real_output_in_tx_index: " << tx_source.real_output_in_tx_index << endl;

                    uint64_t index_of_real_output = tx_source.outputs[tx_source.real_output].first;

                    tx_out_index real_toi;

                    uint64_t tx_source_amount = (tx_source.rct ? 0 : tx_source.amount);

                    try
                    {
                        // get tx of the real output
                        real_toi = core_storage->get_db()
                                .get_output_tx_and_index(tx_source_amount,
                                                         index_of_real_output);
                    }
                    catch (const OUTPUT_DNE& e)
                    {

                        string out_msg = fmt::format(
                                "Output with amount {:d} and index {:d} does not exist!",
                                tx_source_amount, index_of_real_output
                        );

                        cerr << out_msg << endl;

                        return string(out_msg);
                    }



                    transaction real_source_tx;

                    if (!mcore->get_tx(real_toi.first, real_source_tx))
                    {
                        cerr << "Cant get tx in blockchain: " << real_toi.first << endl;
                        return string("Cant get tx: " + pod_to_hex(real_toi.first));
                    }

                    tx_details real_txd = get_tx_details(real_source_tx);

                    real_output_indices.push_back(tx_source.real_output);

                    public_key real_out_pub_key = real_txd.output_pub_keys[tx_source.real_output_in_tx_index].first.key;

                    //cout << "real_txd.hash: "    << pod_to_hex(real_txd.hash) << endl;
                    //cout << "real_txd.pk: "      << pod_to_hex(real_txd.pk) << endl;
                    //cout << "real_out_pub_key: " << pod_to_hex(real_out_pub_key) << endl;

                    mstch::array &outputs = boost::get<mstch::array>(single_dest_source["outputs"]);

                    vector<uint64_t> mixin_timestamps;

                    size_t output_i {0};

                    for(const tx_source_entry::output_entry &oe: tx_source.outputs)
                    {

                        tx_out_index toi;

                        try
                        {

                            // get tx of the real output
                            toi = core_storage->get_db()
                                    .get_output_tx_and_index(tx_source_amount, oe.first);
                        }
                        catch (OUTPUT_DNE& e)
                        {

                            string out_msg = fmt::format(
                                    "Output with amount {:d} and index {:d} does not exist!",
                                    tx_source_amount, oe.first
                            );

                            cerr << out_msg << endl;

                            return string(out_msg);
                        }

                        transaction tx;

                        if (!mcore->get_tx(toi.first, tx))
                        {
                            cerr << "Cant get tx in blockchain: " << toi.first
                                 << ". \n Check mempool now" << endl;
                            // tx is nowhere to be found :-(
                            return string("Cant get tx: " + pod_to_hex(toi.first));
                        }

                        tx_details txd = get_tx_details(tx);

                        public_key out_pub_key = txd.output_pub_keys[toi.second].first.key;


                        // get block cointaining this tx
                        block blk;

                        if (!mcore->get_block_by_height(txd.blk_height, blk))
                        {
                            cerr << "Cant get block: " << txd.blk_height << endl;
                            return string("Cant get block: "  + to_string(txd.blk_height));
                        }

                        pair<string, string> age = get_age(server_timestamp, blk.timestamp);

                        mstch::map single_output {
                                {"out_index"          , oe.first},
                                {"tx_hash"            , pod_to_hex(txd.hash)},
                                {"out_pub_key"        , pod_to_hex(out_pub_key)},
                                {"ctkey"              , pod_to_hex(oe.second)},
                                {"output_age"         , age.first},
                                {"is_real"            , (out_pub_key == real_out_pub_key)}
                        };

                        single_dest_source.insert({"age_format"          , age.second});

                        outputs.push_back(single_output);

                        mixin_timestamps.push_back(blk.timestamp);

                        ++output_i;

                    } // for(const tx_source_entry::output_entry& oe: tx_source.outputs)

                    dest_sources.push_back(single_dest_source);

                    mixin_timestamp_groups.push_back(mixin_timestamps);

                } //  for (size_t i = 0; i < no_of_sources; ++i)

                tx_cd_data.insert({"sum_outputs_amounts" ,
                                   xmreg::arq_amount_to_str(sum_outputs_amounts)});


                uint64_t min_mix_timestamp;
                uint64_t max_mix_timestamp;

                pair<mstch::array, double> mixins_timescales
                        = construct_mstch_mixin_timescales(
                                mixin_timestamp_groups,
                                min_mix_timestamp,
                                max_mix_timestamp
                        );

                tx_cd_data.emplace("timescales", mixins_timescales.first);
                tx_cd_data["min_mix_time"]     = xmreg::timestamp_to_str_gm(min_mix_timestamp);
                tx_cd_data["max_mix_time"]     = xmreg::timestamp_to_str_gm(max_mix_timestamp);
                tx_cd_data["timescales_scale"] = fmt::format("{:0.2f}",
                                                             mixins_timescales.second
                                                             / 3600.0 / 24.0); // in days

                // mark real mixing in the mixins timescale graph
                mark_real_mixins_on_timescales(real_output_indices, tx_cd_data);

                txs.push_back(tx_cd_data);

            } // for (const ::tools::wallet2::tx_construction_data& tx_cd: exported_txs.txes)
        }
        else
        {
            cerr << "deserialization of unsigned tx data NOT successful" << endl;
            return string("deserialization of unsigned tx data NOT successful. "
                                  "Maybe its not base64 encoded?");
        }
    } // if (unsigned_tx_given)
    else
    {
        // if raw data is not unsigined tx, then assume it is signed tx

        const size_t magiclen = strlen(SIGNED_TX_PREFIX);

        string data_prefix = xmreg::make_printable(decoded_raw_tx_data.substr(0, magiclen));

        if (strncmp(decoded_raw_tx_data.c_str(), SIGNED_TX_PREFIX, magiclen) != 0)
        {

            // ok, so its not signed tx data. but maybe it is raw tx data
            // used in rpc call "/sendrawtransaction". This is for example
            // used in mymonero and openmonero projects.

            // to check this, first we need to encode data back to base64.
            // the reason is that txs submited to "/sendrawtransaction"
            // are not base64, and we earlier always asume it is base64.

            // string reencoded_raw_tx_data = epee::string_encoding::base64_decode(raw_tx_data);

            //cout << "raw_tx_data: " << raw_tx_data << endl;

            cryptonote::blobdata tx_data_blob;

            if (!epee::string_tools::parse_hexstr_to_binbuff(raw_tx_data, tx_data_blob))
            {
                string msg = fmt::format("The data is neither unsigned, signed tx or raw tx! "
                                                 "Its prefix is: {:s}",
                                         data_prefix);

                cerr << msg << endl;

                return string(msg);
            }

            crypto::hash tx_hash_from_blob;
            crypto::hash tx_prefix_hash_from_blob;
            cryptonote::transaction tx_from_blob;

//                std::stringstream ss;
//                ss << tx_data_blob;
//                binary_archive<false> ba(ss);
//                serialization::serialize(ba, tx_from_blob);

            if (!cryptonote::parse_and_validate_tx_from_blob(tx_data_blob,
                                                             tx_from_blob,
                                                             tx_hash_from_blob,
                                                             tx_prefix_hash_from_blob))
            {
                string error_msg = fmt::format("failed to validate transaction");

                context["has_error"] = true;
                context["error_msg"] = error_msg;

                return mstch::render(full_page, context);
            }

            //cout << "tx_from_blob.vout.size(): " << tx_from_blob.vout.size() << endl;

            // tx has been correctly deserialized. So
            // we just dispaly it. We dont have any information about real mixins, etc,
            // so there is not much more we can do with tx data.

            mstch::map tx_context = construct_tx_context(tx_from_blob);

            if (boost::get<bool>(tx_context["has_error"]))
            {
                return boost::get<string>(tx_context["error_msg"]);
            }

            // this will be stored in html for for checking outputs
            // we need this data if we want to use "Decode outputs"
            // to see which outputs are ours, and decode amounts in ringct txs
            tx_context["raw_tx_data"]            = raw_tx_data;
            tx_context["show_more_details_link"] = false;

            context["data_prefix"] = string("none as this is pure raw tx data");
            context["tx_json"]     = obj_to_json_str(tx_from_blob);

            context.emplace("txs"     , mstch::array{});

            boost::get<mstch::array>(context["txs"]).push_back(tx_context);

            map<string, string> partials
            {
              {"tx_details", template_file["tx_details"]},
            };

            add_css_style(context);

            // render the page
            return mstch::render(template_file["checkrawtx"], context, partials);

        } // if (strncmp(decoded_raw_tx_data.c_str(), SIGNED_TX_PREFIX, magiclen) != 0)

        context["data_prefix"] = data_prefix;

        bool r {false};

        string s = decoded_raw_tx_data.substr(magiclen);

        ::tools::wallet2::signed_tx_set signed_txs;

        try
        {
            std::istringstream iss(s);
            boost::archive::portable_binary_iarchive ar(iss);
            ar >> signed_txs;

            r = true;
        }
        catch (...)
        {
            cerr << "Failed to parse signed tx data " << endl;
        }

        if (!r)
        {
            cerr << "deserialization of signed tx data NOT successful" << endl;
            return string("deserialization of signed tx data NOT successful. "
                                  "Maybe its not base64 encoded?");
        }

        std::vector<tools::wallet2::pending_tx> ptxs = signed_txs.ptx;

        context.insert({"txs", mstch::array{}});

        for (tools::wallet2::pending_tx &ptx: ptxs)
        {
            mstch::map tx_context = construct_tx_context(ptx.tx, 1);

            if (boost::get<bool>(tx_context["has_error"]))
            {
                return boost::get<string>(tx_context["error_msg"]);
            }

            tx_context["tx_prv_key"] = fmt::format("{:s}", pod_to_hex(unwrap(unwrap(ptx.tx_key))));

            mstch::array destination_addresses;
            vector<uint64_t> real_ammounts;
            uint64_t outputs_arq_sum {0};

            // destiantion address for this tx
            for (tx_destination_entry &a_dest: ptx.construction_data.splitted_dsts)
            {
                //stealth_address_amount.insert({dest.addr, dest.amount});
                //cout << get_account_address_as_str(testnet, a_dest.addr) << endl;
                //address_amounts.push_back(a_dest.amount);

                destination_addresses.push_back(
                        mstch::map {
                                {"dest_address"   , get_account_address_as_str(
                                        nettype, a_dest.is_subaddress, a_dest.addr)},
                                {"dest_amount"    , xmreg::arq_amount_to_str(a_dest.amount)},
                                {"is_this_change" , false}
                        }
                );

                outputs_arq_sum += a_dest.amount;

                real_ammounts.push_back(a_dest.amount);
            }

            // get change address and amount info
            if (ptx.construction_data.change_dts.amount > 0)
            {
                destination_addresses.push_back(
                        mstch::map {
                                {"dest_address"   , get_account_address_as_str(
                                        nettype, ptx.construction_data.change_dts.is_subaddress, ptx.construction_data.change_dts.addr)},
                                {"dest_amount"    ,
                                        xmreg::arq_amount_to_str(ptx.construction_data.change_dts.amount)},
                                {"is_this_change" , true}
                        }
                );

                real_ammounts.push_back(ptx.construction_data.change_dts.amount);
            };

            tx_context["outputs_arq_sum"] = xmreg::arq_amount_to_str(outputs_arq_sum);

            tx_context.insert({"dest_infos", destination_addresses});

            // get reference to inputs array created of the tx
            mstch::array &outputs = boost::get<mstch::array>(tx_context["outputs"]);

            // show real output amount for ringct outputs.
            // otherwise its only 0.000000000
            for (size_t i = 0; i < outputs.size(); ++i)
            {
                mstch::map &output_map = boost::get<mstch::map>(outputs.at(i));

                string &out_amount_str = boost::get<string>(output_map["amount"]);

                //cout << boost::get<string>(output_map["out_pub_key"])
                //    <<", " <<  out_amount_str << endl;

                uint64_t output_amount;

                if (parse_amount(output_amount, out_amount_str))
                {
                    if (output_amount == 0)
                    {
                        out_amount_str = xmreg::arq_amount_to_str(real_ammounts.at(i));
                    }
                }
            }

            // get public keys of real outputs
            vector<string>   real_output_pub_keys;
            vector<uint64_t> real_output_indices;
            vector<uint64_t> real_amounts;

            uint64_t inputs_arq_sum {0};

            for (const tx_source_entry &tx_source: ptx.construction_data.sources)
            {
                transaction real_source_tx;

                uint64_t index_of_real_output = tx_source.outputs[tx_source.real_output].first;

                uint64_t tx_source_amount = (tx_source.rct ? 0 : tx_source.amount);

                tx_out_index real_toi;

                try
                {
                    // get tx of the real output
                    real_toi = core_storage->get_db()
                            .get_output_tx_and_index(tx_source_amount, index_of_real_output);
                }
                catch (const OUTPUT_DNE& e)
                {

                    string out_msg = fmt::format(
                            "Output with amount {:d} and index {:d} does not exist!",
                            tx_source_amount, index_of_real_output
                    );

                    cerr << out_msg << endl;

                    return string(out_msg);
                }

                if (!mcore->get_tx(real_toi.first, real_source_tx))
                {
                    cerr << "Cant get tx in blockchain: " << real_toi.first << endl;
                    return string("Cant get tx: " + pod_to_hex(real_toi.first));
                }

                tx_details real_txd = get_tx_details(real_source_tx);

                public_key real_out_pub_key
                        = real_txd.output_pub_keys[tx_source.real_output_in_tx_index].first.key;

                real_output_pub_keys.push_back(
                        REMOVE_HASH_BRACKETS(fmt::format("{:s}",real_out_pub_key))
                );

                real_output_indices.push_back(tx_source.real_output);
                real_amounts.push_back(tx_source.amount);

                inputs_arq_sum += tx_source.amount;
            }

            // mark that we have signed tx data for use in mstch
            tx_context["have_raw_tx"] = true;

            // provide total mount of inputs xmr
            tx_context["inputs_arq_sum"] = xmreg::arq_amount_to_str(inputs_arq_sum);

            // get reference to inputs array created of the tx
            mstch::array &inputs = boost::get<mstch::array>(tx_context["inputs"]);

            uint64_t input_idx {0};

            // mark which mixin is real in each input's mstch context
            for (mstch::node &input_node: inputs)
            {

                mstch::map &input_map = boost::get<mstch::map>(input_node);

                // show input amount
                string &amount = boost::get<string>(
                        boost::get<mstch::map>(input_node)["amount"]
                );

                amount = xmreg::arq_amount_to_str(real_amounts.at(input_idx));

                // check if key images are spend or not

                string &in_key_img_str = boost::get<string>(
                        boost::get<mstch::map>(input_node)["in_key_img"]
                );

                key_image key_imgage;

                if (epee::string_tools::hex_to_pod(in_key_img_str, key_imgage))
                {
                    input_map["already_spent"] = core_storage->get_db().has_key_image(key_imgage);
                }

                // mark real mixings

                mstch::array &mixins = boost::get<mstch::array>(
                        boost::get<mstch::map>(input_node)["mixins"]
                );

                for (mstch::node &mixin_node: mixins)
                {
                    mstch::map& mixin = boost::get<mstch::map>(mixin_node);

                    string mix_pub_key_str = boost::get<string>(mixin["mix_pub_key"]);

                    //cout << mix_pub_key_str << endl;

                    if (std::find(
                            real_output_pub_keys.begin(),
                            real_output_pub_keys.end(),
                            mix_pub_key_str) != real_output_pub_keys.end())
                    {
                        mixin["mix_is_it_real"] = true;
                    }
                }

                ++input_idx;
            }

            // mark real mixing in the mixins timescale graph
            mark_real_mixins_on_timescales(real_output_indices, tx_context);

            boost::get<mstch::array>(context["txs"]).push_back(tx_context);
        }

    }

    map<string, string> partials
    {
      {"tx_details", template_file["tx_details"]},
    };

    // render the page
    return mstch::render(full_page, context, partials);
}

string
show_pushrawtx(string raw_tx_data, string action)
{
    clean_post_data(raw_tx_data);

    // initalize page template context map
    mstch::map context {
            {"testnet"              , testnet},
            {"stagenet"             , stagenet},
            {"have_raw_tx"          , true},
            {"has_error"            , false},
            {"error_msg"            , string {}},
    };

    // add header and footer
    string full_page = template_file["pushrawtx"];

    add_css_style(context);

    std::vector<tools::wallet2::pending_tx> ptx_vector;

    // first try reading raw_tx_data as a raw hex string
    std::string tx_blob;
    cryptonote::transaction parsed_tx;
    crypto::hash parsed_tx_hash, parsed_tx_prefixt_hash;
    if (epee::string_tools::parse_hexstr_to_binbuff(raw_tx_data, tx_blob) && parse_and_validate_tx_from_blob(tx_blob, parsed_tx, parsed_tx_hash, parsed_tx_prefixt_hash))
    {
        ptx_vector.push_back({});
        ptx_vector.back().tx = parsed_tx;
    }
    // if failed, treat raw_tx_data as base64 encoding of signed_monero_tx
    else
    {
        string decoded_raw_tx_data = epee::string_encoding::base64_decode(raw_tx_data);

        const size_t magiclen = strlen(SIGNED_TX_PREFIX);

        string data_prefix = xmreg::make_printable(decoded_raw_tx_data.substr(0, magiclen));

        context["data_prefix"] = data_prefix;

        if (strncmp(decoded_raw_tx_data.c_str(), SIGNED_TX_PREFIX, magiclen) != 0)
        {
            string error_msg = fmt::format("The data does not appear to be signed raw tx! Data prefix: {:s}",
                                           data_prefix);

            context["has_error"] = true;
            context["error_msg"] = error_msg;

            return mstch::render(full_page, context);
        }

        if (this->enable_pusher == false)
        {
            string error_msg = fmt::format(
                    "Pushing disabled!\n "
                            "Run explorer with --enable-pusher flag to enable it.");

            context["has_error"] = true;
            context["error_msg"] = error_msg;

            return mstch::render(full_page, context);
        }

        bool r {false};

        string s = decoded_raw_tx_data.substr(magiclen);

        ::tools::wallet2::signed_tx_set signed_txs;

        try
        {
            std::istringstream iss(s);
            boost::archive::portable_binary_iarchive ar(iss);
            ar >> signed_txs;

            r = true;
        }
        catch (...)
        {
            cerr << "Failed to parse signed tx data " << endl;
        }


        if (!r)
        {
            string error_msg = fmt::format("Deserialization of signed tx data NOT successful! "
                                                   "Maybe its not base64 encoded?");

            context["has_error"] = true;
            context["error_msg"] = error_msg;

            return mstch::render(full_page, context);
        }

        ptx_vector = signed_txs.ptx;
    }

    context.emplace("txs", mstch::array{});

    mstch::array &txs = boost::get<mstch::array>(context["txs"]);

    // actually commit the transactions
    while (!ptx_vector.empty())
    {
        tools::wallet2::pending_tx &ptx = ptx_vector.back();

        tx_details txd = get_tx_details(ptx.tx);

        string tx_hash_str = REMOVE_HASH_BRACKETS(fmt::format("{:s}", txd.hash));

        mstch::map tx_cd_data {
                {"tx_hash"          , tx_hash_str}
        };

        // check in mempool already contains tx to be submited
        vector<MempoolStatus::mempool_tx> found_mempool_txs;

        search_mempool(txd.hash, found_mempool_txs);

        if (!found_mempool_txs.empty())
        {
            string error_msg = fmt::format("Tx already exist in the mempool: {:s}\n",
                                           tx_hash_str);

            context["has_error"] = true;
            context["error_msg"] = error_msg;

            break;
        }

        // check if tx to be submited already exists in the blockchain
        if (core_storage->have_tx(txd.hash))
        {
            string error_msg = fmt::format("Tx already exist in the blockchain: {:s}\n",
                                           tx_hash_str);

            context["has_error"] = true;
            context["error_msg"] = error_msg;

            break;
        }

        // check if any key images of the tx to be submited are already spend
        vector<key_image> key_images_spent;

        for (const txin_to_key &tx_in: txd.input_key_imgs)
        {
            if (core_storage->have_tx_keyimg_as_spent(tx_in.k_image))
                key_images_spent.push_back(tx_in.k_image);
        }

        if (!key_images_spent.empty())
        {
            string error_msg = fmt::format("Tx with hash {:s} has already spent inputs\n",
                                           tx_hash_str);

            for (key_image &k_img: key_images_spent)
            {
                error_msg += REMOVE_HASH_BRACKETS(fmt::format("{:s}", k_img));
                error_msg += "</br>";
            }

            context["has_error"] = true;
            context["error_msg"] = error_msg;

            break;
        }

        string rpc_error_msg;

        if (this->enable_pusher == false)
        {
            string error_msg = fmt::format(
                    "Pushing signed transactions is disabled. "
                            "Run explorer with --enable-pusher flag to enable it.\n");

            context["has_error"] = true;
            context["error_msg"] = error_msg;

            break;
        }

        if (!rpc.commit_tx(ptx, rpc_error_msg))
        {
            string error_msg = fmt::format(
                    "Submitting signed tx {:s} to daemon failed: {:s}\n",
                    tx_hash_str, rpc_error_msg);

            context["has_error"] = true;
            context["error_msg"] = error_msg;

            break;
        }

        txs.push_back(tx_cd_data);

        // if no exception, remove element from vector
        ptx_vector.pop_back();
    }

    // render the page
    return mstch::render(full_page, context);
}


string
show_rawkeyimgs()
{
    // initalize page template context map
    mstch::map context {
            {"testnet"            , testnet},
            {"stagenet"           , stagenet},
    };

    add_css_style(context);

    // render the page
    return mstch::render(template_file["rawkeyimgs"], context);
}

string
show_rawoutputkeys()
{
    // initalize page template context map
    mstch::map context {
            {"testnet"            , testnet},
            {"stagenet"           , stagenet}
    };

    add_css_style(context);

    // render the page
    return mstch::render(template_file["rawoutputkeys"], context);
}

string
show_checkrawkeyimgs(string raw_data, string viewkey_str)
{

    clean_post_data(raw_data);

    // remove white characters
    boost::trim(viewkey_str);

    string decoded_raw_data = epee::string_encoding::base64_decode(raw_data);
    secret_key prv_view_key;

    // initalize page template context map
    mstch::map context{
            {"testnet"         , testnet},
            {"stagenet"        , stagenet},
            {"has_error"       , false},
            {"error_msg"       , string{}},
    };

    // add header and footer
    string full_page = template_file["checkrawkeyimgs"];

    add_css_style(context);

    if (viewkey_str.empty())
    {
        string error_msg = fmt::format("View key not given. Cant decode "
                                               "the key image data without it!");

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }

    if (!xmreg::parse_str_secret_key(viewkey_str, prv_view_key))
    {
        string error_msg = fmt::format("Cant parse the private key: " + viewkey_str);

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }

    const size_t magiclen = strlen(KEY_IMAGE_EXPORT_FILE_MAGIC);

    string data_prefix = xmreg::make_printable(decoded_raw_data.substr(0, magiclen));

    context["data_prefix"] = data_prefix;

    if (strncmp(decoded_raw_data.c_str(), KEY_IMAGE_EXPORT_FILE_MAGIC, magiclen) != 0)
    {
        string error_msg = fmt::format("This does not seem to be key image export data.");

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }

    // decrypt key images data using private view key
    decoded_raw_data = xmreg::decrypt(
            std::string(decoded_raw_data, magiclen),
            prv_view_key, true);

    if (decoded_raw_data.empty())
    {
        string error_msg = fmt::format("Failed to authenticate key images data. "
                                               "Maybe wrong viewkey was porvided?");

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }

    // header is public spend and keys
    const size_t header_lenght = 2 * sizeof(crypto::public_key);
    const size_t key_img_size  = sizeof(crypto::key_image);
    const size_t record_lenght = key_img_size + sizeof(crypto::signature);
    const size_t chacha_length = sizeof(crypto::chacha_key);

    if (decoded_raw_data.size() < header_lenght)
    {
        string error_msg = fmt::format("Bad data size from submitted key images raw data.");

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);

    }

    // get xmr address stored in this key image file
    const account_public_address* arq_address =
            reinterpret_cast<const account_public_address*>(
                    decoded_raw_data.data());

    address_parse_info address_info {*arq_address, false};


    context.insert({"address"        , REMOVE_HASH_BRACKETS(
            xmreg::print_address(address_info, nettype))});
    context.insert({"viewkey"        , REMOVE_HASH_BRACKETS(
            fmt::format("{:s}", pod_to_hex(unwrap(unwrap(prv_view_key)))))});
    context.insert({"has_total_arq"  , false});
    context.insert({"total_arq"      , string{}});
    context.insert({"key_imgs"       , mstch::array{}});


    size_t no_key_images = (decoded_raw_data.size() - header_lenght) / record_lenght;

    //vector<pair<crypto::key_image, crypto::signature>> signed_key_images;

    mstch::array &key_imgs_ctx = boost::get<mstch::array>(context["key_imgs"]);

    for (size_t n = 0; n < no_key_images; ++n)
    {
        const char* record_ptr = decoded_raw_data.data() + header_lenght + n * record_lenght;

        crypto::key_image key_image
                = *reinterpret_cast<const crypto::key_image*>(record_ptr);

        crypto::signature signature
                = *reinterpret_cast<const crypto::signature*>(record_ptr + key_img_size);

        mstch::map key_img_info {
                {"key_no"              , fmt::format("{:03d}", n)},
                {"key_image"           , pod_to_hex(key_image)},
                {"signature"           , fmt::format("{:s}", signature)},
                {"address"             , xmreg::print_address(
                                            address_info, nettype)},
                {"is_spent"            , core_storage->have_tx_keyimg_as_spent(key_image)},
                {"tx_hash"             , string{}}
        };


        key_imgs_ctx.push_back(key_img_info);

    } // for (size_t n = 0; n < no_key_images; ++n)

    // render the page
    return mstch::render(full_page, context);
}

string
show_checkcheckrawoutput(string raw_data, string viewkey_str)
{
    clean_post_data(raw_data);

    // remove white characters
    boost::trim(viewkey_str);

    string decoded_raw_data = epee::string_encoding::base64_decode(raw_data);
    secret_key prv_view_key;

    // initalize page template context map
    mstch::map context{
            {"testnet"         , testnet},
            {"stagenet"        , stagenet},
            {"has_error"       , false},
            {"error_msg"       , string{}}
    };

    // add header and footer
    string full_page = template_file["checkoutputkeys"];

    add_css_style(context);

    if (viewkey_str.empty())
    {
        string error_msg = fmt::format("View key not given. Cant decode "
                                               "the outputs data without it!");

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }

    if (!xmreg::parse_str_secret_key(viewkey_str, prv_view_key))
    {
        string error_msg = fmt::format("Cant parse the private key: " + viewkey_str);

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }

    const size_t magiclen = strlen(OUTPUT_EXPORT_FILE_MAGIC);

    string data_prefix = xmreg::make_printable(decoded_raw_data.substr(0, magiclen));

    context["data_prefix"] = data_prefix;

    if (strncmp(decoded_raw_data.c_str(), OUTPUT_EXPORT_FILE_MAGIC, magiclen) != 0)
    {
        string error_msg = fmt::format("This does not seem to be output keys export data.");

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }


    // decrypt key images data using private view key
    decoded_raw_data = xmreg::decrypt(
            std::string(decoded_raw_data, magiclen),
            prv_view_key, true);


    if (decoded_raw_data.empty())
    {
        string error_msg = fmt::format("Failed to authenticate outputs data. "
                                               "Maybe wrong viewkey was porvided?");

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }


    // header is public spend and keys
    const size_t header_lenght = 2 * sizeof(crypto::public_key);

    // get xmr address stored in this key image file
    const account_public_address* arq_address =
            reinterpret_cast<const account_public_address*>(
                    decoded_raw_data.data());

    address_parse_info address_info {*arq_address, false, false, crypto::null_hash8};

    context.insert({"address"        , REMOVE_HASH_BRACKETS(
            xmreg::print_address(address_info, nettype))});
    context.insert({"viewkey"        , pod_to_hex(unwrap(unwrap(prv_view_key)))});
    context.insert({"has_total_arq"  , false});
    context.insert({"total_arq"      , string{}});
    context.insert({"output_keys"    , mstch::array{}});

    mstch::array &output_keys_ctx = boost::get<mstch::array>(context["output_keys"]);


    std::vector<tools::wallet2::transfer_details> outputs;

    try
    {
        std::string body(decoded_raw_data, header_lenght);
        std::stringstream iss;
        iss << body;
        boost::archive::portable_binary_iarchive ar(iss);
        //boost::archive::binary_iarchive ar(iss);

        ar >> outputs;

        //size_t n_outputs = m_wallet->import_outputs(outputs);
    }
    catch (const std::exception &e)
    {
        string error_msg = fmt::format("Failed to import outputs: {:s}", e.what());

        context["has_error"] = true;
        context["error_msg"] = error_msg;

        return mstch::render(full_page, context);
    }

    uint64_t total_arq {0};
    uint64_t output_no {0};

    context["are_key_images_known"] = false;

    for (const tools::wallet2::transfer_details &td: outputs)
    {

        const transaction_prefix &txp = td.m_tx;

        txout_to_key txout_key = boost::get<txout_to_key>(
                txp.vout[td.m_internal_output_index].target);

        uint64_t arq_amount = td.amount();

        // if the output is RingCT, i.e., tx version is 2
        // need to decode its amount
        if (td.is_rct())
        {
            // get tx associated with the given output
            transaction tx;

            if (!mcore->get_tx(td.m_txid, tx))
            {
                string error_msg = fmt::format("Cant get tx of hash: {:s}", td.m_txid);

                context["has_error"] = true;
                context["error_msg"] = error_msg;

                return mstch::render(full_page, context);
            }

            public_key tx_pub_key = xmreg::get_tx_pub_key_from_received_outs(tx);
            std::vector<public_key> additional_tx_pub_keys = cryptonote::get_additional_tx_pub_keys_from_extra(tx);

            // cointbase txs have amounts in plain sight.
            // so use amount from ringct, only for non-coinbase txs
            if (!is_coinbase(tx))
            {

                bool r = decode_ringct(tx.rct_signatures,
                                       tx_pub_key,
                                       prv_view_key,
                                       td.m_internal_output_index,
                                       tx.rct_signatures.ecdhInfo[td.m_internal_output_index].mask,
                                       arq_amount);
                r = r || decode_ringct(tx.rct_signatures,
                                       additional_tx_pub_keys[td.m_internal_output_index],
                                       prv_view_key,
                                       td.m_internal_output_index,
                                       tx.rct_signatures.ecdhInfo[td.m_internal_output_index].mask,
                                       arq_amount);

                if (!r)
                {
                    string error_msg = fmt::format(
                            "Cant decode RingCT for output: {:s}",
                            txout_key.key);

                    context["has_error"] = true;
                    context["error_msg"] = error_msg;

                    return mstch::render(full_page, context);
                }

            } //  if (!is_coinbase(tx))

        } // if (td.is_rct())

        uint64_t blk_timestamp = core_storage->get_db().get_block_timestamp(td.m_block_height);

        const key_image* output_key_img;

        bool is_output_spent {false};

        if (td.m_key_image_known)
        {
            //are_key_images_known

            output_key_img = &td.m_key_image;

            is_output_spent = core_storage->have_tx_keyimg_as_spent(*output_key_img);

            context["are_key_images_known"] = true;
        }

        mstch::map output_info {
                {"output_no"           , fmt::format("{:03d}", output_no)},
                {"output_pub_key"      , REMOVE_HASH_BRACKETS(fmt::format("{:s}", txout_key.key))},
                {"amount"              , xmreg::arq_amount_to_str(arq_amount)},
                {"tx_hash"             , REMOVE_HASH_BRACKETS(fmt::format("{:s}", td.m_txid))},
                {"timestamp"           , xmreg::timestamp_to_str_gm(blk_timestamp)},
                {"is_spent"            , is_output_spent},
                {"is_ringct"           , td.m_rct}
        };

        ++output_no;

        if (!is_output_spent)
        {
            total_arq += arq_amount;
        }

        output_keys_ctx.push_back(output_info);
    }

    if (total_arq > 0)
    {
        context["has_total_arq"] = true;
        context["total_arq"] = xmreg::arq_amount_to_str(total_arq);
    }

    return mstch::render(full_page, context);;
}


string
search(string search_text)
{
    // remove white characters
    boost::trim(search_text);

    string default_txt {"No such thing found: " + search_text};

    string result_html {default_txt};

    uint64_t search_str_length = search_text.length();

    // first let try searching for tx
    result_html = show_tx(search_text);

    // nasty check if output is "Cant get" as a sign of
    // a not found tx. Later need to think of something better.
    if (result_html.find("Cant get") == string::npos)
    {
        return result_html;
    }


    // first check if searching for block of given height
    if (search_text.size() < 12)
    {
        uint64_t blk_height;

        try
        {
            blk_height = boost::lexical_cast<uint64_t>(search_text);

            result_html = show_block(blk_height);

            // nasty check if output is "Cant get" as a sign of
            // a not found tx. Later need to think of something better.
            if (result_html.find("Cant get") == string::npos)
            {
                return result_html;
            }
        }
        catch(boost::bad_lexical_cast &e)
        {
            cerr << fmt::format("Parsing {:s} into uint64_t failed", search_text)
                 << endl;
        }
    }

    // if tx search not successful, check if we are looking
    // for a block with given hash
    result_html = show_block(search_text);

    if (result_html.find("Cant get") == string::npos)
    {
        return result_html;
    }

    result_html = default_txt;


    // check if Arqma address is given based on its length
    // if yes, then we can only show its public components
    if (search_str_length == 97)
    {
        // parse string representing given monero address
        address_parse_info address_info;

        cryptonote::network_type nettype_addr {cryptonote::network_type::MAINNET};

        if (search_text[0] == 'a' && search_text[1] == 't')
            nettype_addr = cryptonote::network_type::TESTNET;
        if (search_text[0] == 'a' && search_text[1] == 's')
            nettype_addr = cryptonote::network_type::STAGENET;

        if (!xmreg::parse_str_address(search_text, address_info, nettype_addr))
        {
            cerr << "Cant parse string address: " << search_text << endl;
            return string("Cant parse address (probably incorrect format): ")
                   + search_text;
        }

        return show_address_details(address_info, nettype_addr);
    }

    // check if integrated Arqma address is given based on its length
    // if yes, then show its public components search tx based on encrypted id
    if (search_str_length == 109)
    {

        cryptonote::account_public_address address;

        address_parse_info address_info;

        if(!get_account_address_from_str(address_info, nettype, search_text))
        {
            cerr << "Cant parse string integerated address: " << search_text << endl;
            return string("Cant parse address (probably incorrect format): ")
                   + search_text;
        }

        return show_integrated_address_details(address_info,
                                               address_info.payment_id,
                                               nettype);
    }

    // all_possible_tx_hashes was field using custom lmdb database
    // it was dropped, so all_possible_tx_hashes will be alwasy empty
    // for now
    vector<pair<string, vector<string>>> all_possible_tx_hashes;

    result_html = show_search_results(search_text, all_possible_tx_hashes);

    return result_html;
}

string
show_address_details(const address_parse_info& address_info, cryptonote::network_type nettype = cryptonote::network_type::MAINNET)
{

    string address_str      = xmreg::print_address(address_info, nettype);
    string pub_viewkey_str  = fmt::format("{:s}", address_info.address.m_view_public_key);
    string pub_spendkey_str = fmt::format("{:s}", address_info.address.m_spend_public_key);

    mstch::map context {
            {"arq_address"        , REMOVE_HASH_BRACKETS(address_str)},
            {"public_viewkey"     , REMOVE_HASH_BRACKETS(pub_viewkey_str)},
            {"public_spendkey"    , REMOVE_HASH_BRACKETS(pub_spendkey_str)},
            {"is_integrated_addr" , false},
            {"testnet"            , testnet},
            {"stagenet"           , stagenet},
    };

    add_css_style(context);

    // render the page
    return mstch::render(template_file["address"], context);
}

// ;
string
show_integrated_address_details(const address_parse_info& address_info,
                                const crypto::hash8& encrypted_payment_id,
                                cryptonote::network_type nettype = cryptonote::network_type::MAINNET)
{

    string address_str        = xmreg::print_address(address_info, nettype);
    string pub_viewkey_str    = fmt::format("{:s}", address_info.address.m_view_public_key);
    string pub_spendkey_str   = fmt::format("{:s}", address_info.address.m_spend_public_key);
    string enc_payment_id_str = fmt::format("{:s}", encrypted_payment_id);

    mstch::map context {
            {"arq_address"          , REMOVE_HASH_BRACKETS(address_str)},
            {"public_viewkey"       , REMOVE_HASH_BRACKETS(pub_viewkey_str)},
            {"public_spendkey"      , REMOVE_HASH_BRACKETS(pub_spendkey_str)},
            {"encrypted_payment_id" , REMOVE_HASH_BRACKETS(enc_payment_id_str)},
            {"is_integrated_addr"   , true},
            {"testnet"              , testnet},
            {"stagenet"             , stagenet},
    };

    add_css_style(context);

    // render the page
    return mstch::render(template_file["address"], context);
}

map<string, vector<string>>
search_txs(vector<transaction> txs, const string& search_text)
{
    map<string, vector<string>> tx_hashes;

    // initlizte the map with empty results
    tx_hashes["key_images"]                             = {};
    tx_hashes["tx_public_keys"]                         = {};
    tx_hashes["payments_id"]                            = {};
    tx_hashes["encrypted_payments_id"]                  = {};
    tx_hashes["output_public_keys"]                     = {};

    for (const transaction& tx: txs)
    {

        tx_details txd = get_tx_details(tx);

        string tx_hash_str = pod_to_hex(txd.hash);

        // check if any key_image matches the search_text

        vector<txin_to_key>::iterator it1 =
                find_if(begin(txd.input_key_imgs), end(txd.input_key_imgs),
                        [&](const txin_to_key &key_img)
                        {
                            return pod_to_hex(key_img.k_image) == search_text;
                        });

        if (it1 != txd.input_key_imgs.end())
        {
            tx_hashes["key_images"].push_back(tx_hash_str);
        }

        // check if  tx_public_key matches the search_text

        if (pod_to_hex(txd.pk) == search_text)
        {
            tx_hashes["tx_public_keys"].push_back(tx_hash_str);
        }

        // check if  payments_id matches the search_text

        if (pod_to_hex(txd.payment_id) == search_text)
        {
            tx_hashes["payment_id"].push_back(tx_hash_str);
        }

        // check if  encrypted_payments_id matches the search_text

        if (pod_to_hex(txd.payment_id8) == search_text)
        {
            tx_hashes["encrypted_payments_id"].push_back(tx_hash_str);
        }

        // check if output_public_keys matche the search_text

        vector<pair<txout_to_key, uint64_t>>::iterator it2 =
                find_if(begin(txd.output_pub_keys), end(txd.output_pub_keys),
                        [&](const pair<txout_to_key, uint64_t> &tx_out_pk)
                        {
                            return pod_to_hex(tx_out_pk.first.key) == search_text;
                        });

        if (it2 != txd.output_pub_keys.end())
        {
            tx_hashes["output_public_keys"].push_back(tx_hash_str);
        }

    }

    return  tx_hashes;

}

string
show_search_results(const string &search_text,
                    const vector<pair<string, vector<string>>> &all_possible_tx_hashes)
{

    // initalise page tempate map with basic info about blockchain
    mstch::map context {
            {"testnet"         , testnet},
            {"stagenet"        , stagenet},
            {"search_text"     , search_text},
            {"no_results"      , true},
            {"to_many_results" , false}
    };

    for (const pair<string, vector<string>> &found_txs: all_possible_tx_hashes)
    {
        // define flag, e.g., has_key_images denoting that
        // tx hashes for key_image searched were found
        context.insert({"has_" + found_txs.first, !found_txs.second.empty()});

        // cout << "found_txs.first: " << found_txs.first << endl;

        // insert new array based on what we found to context if not exist
        pair<mstch::map::iterator, bool> res
                = context.insert({found_txs.first, mstch::array{}});

        if (!found_txs.second.empty())
        {

            uint64_t tx_i {0};

            // for each found tx_hash, get the corresponding tx
            // and its details, and put into mstch for rendering
            for (const string &tx_hash: found_txs.second)
            {

                crypto::hash tx_hash_pod;

                epee::string_tools::hex_to_pod(tx_hash, tx_hash_pod);

                transaction tx;

                uint64_t blk_height {0};

                int64_t blk_timestamp;

                // first check in the blockchain
                if (mcore->get_tx(tx_hash, tx))
                {

                    // get timestamp of the tx's block
                    blk_height = core_storage->get_db().get_tx_block_height(tx_hash_pod);

                    blk_timestamp = core_storage->get_db().get_block_timestamp(blk_height);

                }
                else
                {
                    // check in mempool if tx_hash not found in the
                    // blockchain
                    vector<MempoolStatus::mempool_tx> found_txs;

                    search_mempool(tx_hash_pod, found_txs);

                    if (!found_txs.empty())
                    {
                        // there should be only one tx found
                        tx = found_txs.at(0).tx;
                    }
                    else
                    {
                        return string("Cant get tx of hash (show_search_results): " + tx_hash);
                    }

                    // tx in mempool have no blk_timestamp
                    // but can use their recive time
                    blk_timestamp = found_txs.at(0).receive_time;

                }

                tx_details txd = get_tx_details(tx);

                mstch::map txd_map = txd.get_mstch_map();


                // add the timestamp to tx mstch map
                txd_map.insert({"timestamp", xmreg::timestamp_to_str_gm(blk_timestamp)});

                boost::get<mstch::array>((res.first)->second).push_back(txd_map);

                // dont show more than 500 results
                if (tx_i > 500)
                {
                    context["to_many_results"] = true;
                    break;
                }

                ++tx_i;
            }

            // if found something, set this flag to indicate this fact
            context["no_results"] = false;
        }
    }

    // add header and footer
    string full_page = template_file["search_results"];

    map<string, string> partials
    {
      {"tx_table_head", template_file["tx_table_header"]},
      {"tx_table_row", template_file["tx_table_row"]},
    };

    add_css_style(context);

    // render the page
    return  mstch::render(full_page, context, partials);
}

/*
 * Lets use this json api convention for success and error
 * https://labs.omniti.com/labs/jsend
 */
json
json_transaction(string tx_hash_str)
{
    json j_response {
            {"status", "fail"},
            {"data"  , json {}}
    };

    json& j_data = j_response["data"];

    // parse tx hash string to hash object
    crypto::hash tx_hash;

    if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
    {
        j_data["title"] = fmt::format("Cant parse tx hash: {:s}", tx_hash_str);
        return j_response;
    }

    // get transaction
    transaction tx;

    // flag to indicate if tx is in mempool
    bool found_in_mempool {false};

    // for tx in blocks we get block timestamp
    // for tx in mempool we get recievive time
    uint64_t tx_timestamp {0};

    if (!find_tx(tx_hash, tx, found_in_mempool, tx_timestamp))
    {
        j_data["title"] = fmt::format("Cant find tx hash: {:s}", tx_hash_str);
        return j_response;
    }

    uint64_t block_height {0};
    uint64_t is_coinbase_tx = is_coinbase(tx);
    uint64_t no_confirmations {0};

    if (found_in_mempool == false)
    {

        block blk;

        try
        {
            // get block cointaining this tx
            block_height = core_storage->get_db().get_tx_block_height(tx_hash);

            if (!mcore->get_block_by_height(block_height, blk))
            {
                j_data["title"] = fmt::format("Cant get block: {:d}", block_height);
                return j_response;
            }

            tx_timestamp = blk.timestamp;
        }
        catch (const exception &e)
        {
            j_response["status"]  = "error";
            j_response["message"] = fmt::format("Tx does not exist in blockchain, "
                                                        "but was there before: {:s}", tx_hash_str);
            return j_response;
        }
    }

    string blk_timestamp_utc = xmreg::timestamp_to_str_gm(tx_timestamp);

    // get the current blockchain height. Just to check
    uint64_t bc_height = core_storage->get_current_blockchain_height();

    tx_details txd = get_tx_details(tx, is_coinbase_tx, block_height, bc_height);

    json outputs;

    for (const auto &output: txd.output_pub_keys)
    {
        outputs.push_back(json {
                {"public_key", pod_to_hex(output.first.key)},
                {"amount"    , output.second}
        });
    }

    json inputs;

    for (const txin_to_key &in_key: txd.input_key_imgs)
    {

        // get absolute offsets of mixins
        std::vector<uint64_t> absolute_offsets
                = cryptonote::relative_output_offsets_to_absolute(
                        in_key.key_offsets);

        // get public keys of outputs used in the mixins that match to the offests
        std::vector<output_data_t> outputs;

        try
        {
            // before proceeding with geting the outputs based on the amount and absolute offset
            // check how many outputs there are for that amount
            // go to next input if a too large offset was found
            if (are_absolute_offsets_good(absolute_offsets, in_key) == false)
                continue;

            core_storage->get_db().get_output_key(epee::span<const uint64_t>(&in_key.amount, 1),
                                                  absolute_offsets,
                                                  outputs);
        }
        catch (const OUTPUT_DNE &e)
        {
            j_response["status"]  = "error";
            j_response["message"] = "Failed to retrive outputs (mixins) used in key images";
            return j_response;
        }

        inputs.push_back(json {
                {"key_image"  , pod_to_hex(in_key.k_image)},
                {"amount"     , in_key.amount},
                {"mixins"     , json {}}
        });

        json& mixins = inputs.back()["mixins"];

        for (const output_data_t &output_data: outputs)
        {
            mixins.push_back(json {
                    {"public_key"  , pod_to_hex(output_data.pubkey)},
                    {"block_no"    , output_data.height},
            });
        }
    }

    if (found_in_mempool == false)
    {
        no_confirmations = txd.no_confirmations;
    }

    // get basic tx info
    j_data = get_tx_json(tx, txd);

    // append additional info from block, as we don't
    // return block data in this function
    j_data["timestamp"]      = tx_timestamp;
    j_data["timestamp_utc"]  = blk_timestamp_utc;
    j_data["block_height"]   = block_height;
    j_data["confirmations"]  = no_confirmations;
    j_data["outputs"]        = outputs;
    j_data["inputs"]         = inputs;
    j_data["current_height"] = bc_height;

    j_response["status"] = "success";

    return j_response;
}



/*
 * Lets use this json api convention for success and error
 * https://labs.omniti.com/labs/jsend
 */
json
json_rawtransaction(string tx_hash_str)
{
    json j_response {
            {"status", "fail"},
            {"data"  , json {}}
    };

    json& j_data = j_response["data"];

    // parse tx hash string to hash object
    crypto::hash tx_hash;

    if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
    {
        j_data["title"] = fmt::format("Cant parse tx hash: {:s}", tx_hash_str);
        return j_response;
    }

    // get transaction
    transaction tx;

    // flag to indicate if tx is in mempool
    bool found_in_mempool {false};

    // for tx in blocks we get block timestamp
    // for tx in mempool we get recievive time
    uint64_t tx_timestamp {0};

    if (!find_tx(tx_hash, tx, found_in_mempool, tx_timestamp))
    {
        j_data["title"] = fmt::format("Cant find tx hash: {:s}", tx_hash_str);
        return j_response;
    }

    if (found_in_mempool == false)
    {

        block blk;

        try
        {
            // get block cointaining this tx
            uint64_t block_height = core_storage->get_db().get_tx_block_height(tx_hash);

            if (!mcore->get_block_by_height(block_height, blk))
            {
                j_data["title"] = fmt::format("Cant get block: {:d}", block_height);
                return j_response;
            }
        }
        catch (const exception &e)
        {
            j_response["status"]  = "error";
            j_response["message"] = fmt::format("Tx does not exist in blockchain, "
                                                "but was there before: {:s}",
                                                tx_hash_str);
            return j_response;
        }
    }

    // get raw tx json as in monero

    try
    {
        j_data = json::parse(obj_to_json_str(tx));
    }
    catch (std::invalid_argument &e)
    {
        j_response["status"]  = "error";
        j_response["message"] = "Faild parsing raw tx data into json";
        return j_response;
    }

    j_response["status"] = "success";

    return j_response;
}


json
json_detailedtransaction(string tx_hash_str)
{
    json j_response {
            {"status", "fail"},
            {"data"  , json {}}
    };

    json& j_data = j_response["data"];

    transaction tx;

    bool found_in_mempool {false};
    uint64_t tx_timestamp {0};
    string error_message;

    if (!find_tx_for_json(tx_hash_str, tx, found_in_mempool, tx_timestamp, error_message))
    {
        j_data["title"] = error_message;
        return j_response;
    }

    // get detailed tx information
    mstch::map tx_context = construct_tx_context(tx, 1 /*full detailed */);

    // remove some page specific and html stuff
    tx_context.erase("timescales");
    tx_context.erase("tx_json");
    tx_context.erase("tx_json_raw");
    tx_context.erase("enable_mixins_details");
    tx_context.erase("with_ring_signatures");
    tx_context.erase("show_part_of_inputs");
    tx_context.erase("show_more_details_link");
    tx_context.erase("max_no_of_inputs_to_show");
    tx_context.erase("inputs_arq_sum_not_zero");
    tx_context.erase("have_raw_tx");
    tx_context.erase("have_any_unknown_amount");
    tx_context.erase("has_error");
    tx_context.erase("error_msg");
    tx_context.erase("server_time");
    tx_context.erase("construction_time");

    j_data = tx_context;

    j_response["status"] = "success";

    return j_response;
}

/*
 * Lets use this json api convention for success and error
 * https://labs.omniti.com/labs/jsend
 */
json
json_block(string block_no_or_hash)
{
    json j_response {
            {"status", "fail"},
            {"data"  , json {}}
    };

    nlohmann::json& j_data = j_response["data"];

    uint64_t current_blockchain_height = core_storage->get_current_blockchain_height();

    uint64_t block_height {0};

    crypto::hash blk_hash;

    block blk;

    if (block_no_or_hash.length() <= 8)
    {
        // we have something that seems to be a block number
        try
        {
            block_height  = boost::lexical_cast<uint64_t>(block_no_or_hash);
        }
        catch (const boost::bad_lexical_cast &e)
        {
            j_data["title"] = fmt::format(
                    "Cant parse block number: {:s}", block_no_or_hash);
            return j_response;
        }

        if (block_height > current_blockchain_height)
        {
            j_data["title"] = fmt::format(
                    "Requested block is higher than blockchain:"
                            " {:d}, {:d}", block_height,current_blockchain_height);
            return j_response;
        }

        if (!mcore->get_block_by_height(block_height, blk))
        {
            j_data["title"] = fmt::format("Cant get block: {:d}", block_height);
            return j_response;
        }

        blk_hash = core_storage->get_block_id_by_height(block_height);

    }
    else if (block_no_or_hash.length() == 64)
    {
        // this seems to be block hash
        if (!xmreg::parse_str_secret_key(block_no_or_hash, blk_hash))
        {
            j_data["title"] = fmt::format("Cant parse blk hash: {:s}", block_no_or_hash);
            return j_response;
        }

        if (!core_storage->get_block_by_hash(blk_hash, blk))
        {
            j_data["title"] = fmt::format("Cant get block: {:s}", blk_hash);
            return j_response;
        }

        block_height = core_storage->get_db().get_block_height(blk_hash);
    }
    else
    {
        j_data["title"] = fmt::format("Cant find blk using search string: {:s}", block_no_or_hash);
        return j_response;
    }


    // get block size in bytes
    uint64_t blk_size = core_storage->get_db().get_block_weight(block_height);

    uint64_t blk_diff;
    if (!mcore->get_diff_at_height(block_height, blk_diff))
    {
        j_data["title"] = fmt::format("Cant get block diff {:d}!", block_height);
        return j_response;
    }

    // miner reward tx
    transaction coinbase_tx = blk.miner_tx;

    // transcation in the block
    vector<crypto::hash> tx_hashes = blk.tx_hashes;

    // sum of all transactions in the block
    uint64_t sum_fees = 0;

    // get tx details for the coinbase tx, i.e., miners reward
    tx_details txd_coinbase = get_tx_details(blk.miner_tx, true,
                                             block_height,
                                             current_blockchain_height);

    json j_txs;

    j_txs.push_back(get_tx_json(coinbase_tx, txd_coinbase));

    // for each transaction in the block
    for (size_t i = 0; i < blk.tx_hashes.size(); ++i)
    {
        const crypto::hash &tx_hash = blk.tx_hashes.at(i);

        // get transaction
        transaction tx;

        if (!mcore->get_tx(tx_hash, tx))
        {
            j_response["status"]  = "error";
            j_response["message"]
                    = fmt::format("Cant get transactions in block: {:d}", block_height);
            return j_response;
        }

        tx_details txd = get_tx_details(tx, false,
                                        block_height,
                                        current_blockchain_height);

        j_txs.push_back(get_tx_json(tx, txd));

        // add fee to the rest
        sum_fees += txd.fee;
    }

    j_data = json {
            {"block_height"  , block_height},
            {"diff"          , blk_diff},
            {"hash"          , pod_to_hex(blk_hash)},
            {"timestamp"     , blk.timestamp},
            {"timestamp_utc" , xmreg::timestamp_to_str_gm(blk.timestamp)},
            {"block_height"  , block_height},
            {"size"          , blk_size},
            {"txs"           , j_txs},
            {"current_height", current_blockchain_height}
    };

    j_response["status"] = "success";

    return j_response;
}



/*
 * Lets use this json api convention for success and error
 * https://labs.omniti.com/labs/jsend
 */
json
json_rawblock(string block_no_or_hash)
{
    json j_response {
            {"status", "fail"},
            {"data"  , json {}}
    };

    json& j_data = j_response["data"];

    uint64_t current_blockchain_height
            =  core_storage->get_current_blockchain_height();

    uint64_t block_height {0};

    crypto::hash blk_hash;

    block blk;

    if (block_no_or_hash.length() <= 8)
    {
        // we have something that seems to be a block number
        try
        {
            block_height = boost::lexical_cast<uint64_t>(block_no_or_hash);
        }
        catch (const boost::bad_lexical_cast &e)
        {
            j_data["title"] = fmt::format(
                    "Cant parse block number: {:s}", block_no_or_hash);
            return j_response;
        }

        if (block_height > current_blockchain_height)
        {
            j_data["title"] = fmt::format(
                    "Requested block is higher than blockchain:"
                            " {:d}, {:d}", block_height,current_blockchain_height);
            return j_response;
        }

        if (!mcore->get_block_by_height(block_height, blk))
        {
            j_data["title"] = fmt::format("Cant get block: {:d}", block_height);
            return j_response;
        }

        blk_hash = core_storage->get_block_id_by_height(block_height);

    }
    else if (block_no_or_hash.length() == 64)
    {
        // this seems to be block hash
        if (!xmreg::parse_str_secret_key(block_no_or_hash, blk_hash))
        {
            j_data["title"] = fmt::format("Cant parse blk hash: {:s}", block_no_or_hash);
            return j_response;
        }

        if (!core_storage->get_block_by_hash(blk_hash, blk))
        {
            j_data["title"] = fmt::format("Cant get block: {:s}", blk_hash);
            return j_response;
        }

        block_height = core_storage->get_db().get_block_height(blk_hash);
    }
    else
    {
        j_data["title"] = fmt::format("Cant find blk using search string: {:s}", block_no_or_hash);
        return j_response;
    }

    // get raw tx json as in Arqma

    try
    {
        j_data = json::parse(obj_to_json_str(blk));
    }
    catch (std::invalid_argument &e)
    {
        j_response["status"]  = "error";
        j_response["message"] = "Faild parsing raw blk data into json";
        return j_response;
    }

    j_response["status"] = "success";

    return j_response;
}


/*
 * Lets use this json api convention for success and error
 * https://labs.omniti.com/labs/jsend
 */
json
json_transactions(string _page, string _limit)
{
    json j_response {
            {"status", "fail"},
            {"data",   json {}}
    };

    json& j_data = j_response["data"];

    // parse page and limit into numbers

    uint64_t page {0};
    uint64_t limit {0};

    try
    {
        page  = boost::lexical_cast<uint64_t>(_page);
        limit = boost::lexical_cast<uint64_t>(_limit);
    }
    catch (const boost::bad_lexical_cast &e)
    {
        j_data["title"] = fmt::format(
                "Cant parse page and/or limit numbers: {:s}, {:s}", _page, _limit);
        return j_response;
    }

    // enforce maximum number of blocks per page to 100
    limit = limit > 100 ? 100 : limit;

    //get current server timestamp
    server_timestamp = std::time(nullptr);

    uint64_t local_copy_server_timestamp = server_timestamp;

    uint64_t height = core_storage->get_current_blockchain_height();

    // calculate starting and ending block numbers to show
    int64_t start_height = height - limit * (page + 1);

    // check if start height is not below range
    start_height = start_height < 0 ? 0 : start_height;

    int64_t end_height = start_height + limit - 1;

    // loop index
    int64_t i = end_height;

    j_data["blocks"] = json::array();
    json& j_blocks = j_data["blocks"];

    // iterate over last no_of_last_blocks of blocks
    while (i >= start_height)
    {
        // get block at the given height i
        block blk;

        if (!mcore->get_block_by_height(i, blk))
        {
            j_response["status"]  = "error";
            j_response["message"] = fmt::format("Cant get block: {:d}", i);
            return j_response;
        }

        uint64_t blk_diff;
        if (!mcore->get_diff_at_height(i, blk_diff))
        {
            cerr << "Cant get block diff: " << i << endl;
            return fmt::format("Cant get block diff {:d}!", i);
        }

        // get block size in bytes
        double blk_size = core_storage->get_db().get_block_weight(i);

        crypto::hash blk_hash = core_storage->get_block_id_by_height(i);

        // get block age
        pair<string, string> age = get_age(local_copy_server_timestamp, blk.timestamp);

        j_blocks.push_back(json {
                {"height"       , i},
                {"hash"         , pod_to_hex(blk_hash)},
                {"age"          , age.first},
                {"diff"         , blk_diff},
                {"size"         , blk_size},
                {"timestamp"    , blk.timestamp},
                {"timestamp_utc", xmreg::timestamp_to_str_gm(blk.timestamp)},
                {"txs"          , json::array()}
        });

        json& j_txs = j_blocks.back()["txs"];

        vector<cryptonote::transaction> blk_txs {blk.miner_tx};
        vector<crypto::hash> missed_txs;

        if (!core_storage->get_transactions(blk.tx_hashes, blk_txs, missed_txs))
        {
            j_response["status"]  = "error";
            j_response["message"] = fmt::format("Cant get transactions in block: {:d}", i);
            return j_response;
        }

        (void) missed_txs;

        for(auto it = blk_txs.begin(); it != blk_txs.end(); ++it)
        {
            const cryptonote::transaction &tx = *it;

            const tx_details &txd = get_tx_details(tx, false, i, height);

            j_txs.push_back(get_tx_json(tx, txd));
        }

        --i;
    }

    j_data["page"]           = page;
    j_data["limit"]          = limit;
    j_data["current_height"] = height;

    j_data["total_page_no"]  = limit > 0 ? (height / limit) : 0;

    j_response["status"] = "success";

    return j_response;
}


/*
* Lets use this json api convention for success and error
* https://labs.omniti.com/labs/jsend
*/
json
json_mempool(string _page, string _limit)
{
    json j_response {
            {"status", "fail"},
            {"data",   json {}}
    };

    json& j_data = j_response["data"];

    // parse page and limit into numbers

    uint64_t page {0};
    uint64_t limit {0};

    try
    {
        page  = boost::lexical_cast<uint64_t>(_page);
        limit = boost::lexical_cast<uint64_t>(_limit);
    }
    catch (const boost::bad_lexical_cast &e)
    {
        j_data["title"] = fmt::format(
                "Cant parse page and/or limit numbers: {:s}, {:s}", _page, _limit);
        return j_response;
    }

    //get current server timestamp
    server_timestamp = std::time(nullptr);

    uint64_t local_copy_server_timestamp = server_timestamp;

    uint64_t height = core_storage->get_current_blockchain_height();

    // get mempool tx from mempoolstatus thread
    vector<MempoolStatus::mempool_tx> mempool_data
            = MempoolStatus::get_mempool_txs();

    uint64_t no_mempool_txs = mempool_data.size();

    // calculate starting and ending block numbers to show
    int64_t start_height = limit * page;

    int64_t end_height = start_height + limit;

    end_height = end_height > no_mempool_txs ? no_mempool_txs : end_height;

    // check if start height is not below range
    start_height = start_height > end_height ? end_height - limit : start_height;

    start_height = start_height < 0 ? 0 : start_height;

    // loop index
    int64_t i = start_height;

    json j_txs = json::array();

    // for each transaction in the memory pool in current page
    while (i < end_height)
    {
        const MempoolStatus::mempool_tx* mempool_tx {nullptr};

        try
        {
            mempool_tx = &(mempool_data.at(i));
        }
        catch (const std::out_of_range &e)
        {
            j_response["status"]  = "error";
            j_response["message"] = fmt::format("Getting mempool txs failed due to std::out_of_range");

            return j_response;
        }

        const tx_details& txd = get_tx_details(mempool_tx->tx, false, 1, height); // 1 is dummy here

        // get basic tx info
        json j_tx = get_tx_json(mempool_tx->tx, txd);

        // we add some extra data, for mempool txs, such as recieve timestamp
        j_tx["timestamp"]     = mempool_tx->receive_time;
        j_tx["timestamp_utc"] = mempool_tx->timestamp_str;

        j_txs.push_back(j_tx);

       ++i;
    }

    j_data["txs"]            = j_txs;
    j_data["page"]           = page;
    j_data["limit"]          = limit;
    j_data["txs_no"]         = no_mempool_txs;
    j_data["total_page_no"]  = limit > 0 ? (no_mempool_txs / limit) : 0;

    j_response["status"] = "success";

    return j_response;
}


/*
 * Lets use this json api convention for success and error
 * https://labs.omniti.com/labs/jsend
 */
json
json_search(const string &search_text)
{
    json j_response {
            {"status", "fail"},
            {"data",   json {}}
    };

    json& j_data = j_response["data"];

    //get current server timestamp
    server_timestamp = std::time(nullptr);

    uint64_t local_copy_server_timestamp = server_timestamp;

    uint64_t height = core_storage->get_current_blockchain_height();

    uint64_t search_str_length = search_text.length();

    // first let check if the search_text matches any tx or block hash
    if (search_str_length == 64)
    {
        // first check for tx
        json j_tx = json_transaction(search_text);

        if (j_tx["status"] == "success")
        {
            j_response["data"]   = j_tx["data"];
            j_response["data"]["title"]  = "transaction";
            j_response["status"] = "success";
            return j_response;
        }

        // now check for block

        json j_block = json_block(search_text);

        if (j_block["status"] == "success")
        {
            j_response["data"]  = j_block["data"];
            j_response["data"]["title"]  = "block";
            j_response["status"] = "success";
            return j_response;
        }
    }

    // now lets see if this is a block number
    if (search_str_length <= 8)
    {
        json j_block = json_block(search_text);

        if (j_block["status"] == "success")
        {
            j_response["data"]   = j_block["data"];
            j_response["data"]["title"]  = "block";
            j_response["status"] = "success";
            return j_response;
        }
    }

    j_data["title"] = "Nothing was found that matches search string: " + search_text;

    return j_response;
}

json
json_outputs(string tx_hash_str,
             string address_str,
             string viewkey_str,
             bool tx_prove = false)
{
    boost::trim(tx_hash_str);
    boost::trim(address_str);
    boost::trim(viewkey_str);

    json j_response {
            {"status", "fail"},
            {"data",   json {}}
    };

    json& j_data = j_response["data"];


    if (tx_hash_str.empty())
    {
        j_response["status"]  = "error";
        j_response["message"] = "Tx hash not provided";
        return j_response;
    }

    if (address_str.empty())
    {
        j_response["status"]  = "error";
        j_response["message"] = "Arqma address not provided";
        return j_response;
    }

    if (viewkey_str.empty())
    {
        if (!tx_prove)
        {
            j_response["status"]  = "error";
            j_response["message"] = "Viewkey not provided";
            return j_response;
        }
        else
        {
            j_response["status"]  = "error";
            j_response["message"] = "Tx private key not provided";
            return j_response;
        }
    }


    // parse tx hash string to hash object
    crypto::hash tx_hash;

    if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
    {
        j_response["status"]  = "error";
        j_response["message"] = "Cant parse tx hash: " + tx_hash_str;
        return j_response;
    }

    // parse string representing given monero address
    address_parse_info address_info;

    if (!xmreg::parse_str_address(address_str,  address_info, nettype))
    {
        j_response["status"]  = "error";
        j_response["message"] = "Cant parse Arqma address: " + address_str;
        return j_response;

    }

    // parse string representing given private key
    crypto::secret_key prv_view_key;

    if (!xmreg::parse_str_secret_key(viewkey_str, prv_view_key))
    {
        j_response["status"]  = "error";
        j_response["message"] = "Cant parse view key or tx private key: "
                                + viewkey_str;
        return j_response;
    }

    // get transaction
    transaction tx;

    // flag to indicate if tx is in mempool
    bool found_in_mempool {false};

    // for tx in blocks we get block timestamp
    // for tx in mempool we get recievive time
    uint64_t tx_timestamp {0};

    if (!find_tx(tx_hash, tx, found_in_mempool, tx_timestamp))
    {
        j_data["title"] = fmt::format("Cant find tx hash: {:s}", tx_hash_str);
        return j_response;
    }

    (void) tx_timestamp;
    (void) found_in_mempool;

    tx_details txd = get_tx_details(tx);

    // public transaction key is combined with our viewkey
    // to create, so called, derived key.
    key_derivation derivation;
    std::vector<key_derivation> additional_derivations(txd.additional_pks.size());

    public_key pub_key = tx_prove ? address_info.address.m_view_public_key : txd.pk;

    //cout << "txd.pk: " << pod_to_hex(txd.pk) << endl;

    if (!generate_key_derivation(pub_key, prv_view_key, derivation))
    {
        j_response["status"]  = "error";
        j_response["message"] = "Cant calculate key_derivation";
        return j_response;
    }
    for (size_t i = 0; i < txd.additional_pks.size(); ++i)
    {
        if (!generate_key_derivation(txd.additional_pks[i], prv_view_key, additional_derivations[i]))
        {
            j_response["status"]  = "error";
            j_response["message"] = "Cant calculate key_derivation";
            return j_response;
        }
    }

    uint64_t output_idx {0};

    std::vector<uint64_t> money_transfered(tx.vout.size(), 0);

    j_data["outputs"] = json::array();
    json& j_outptus   = j_data["outputs"];

    for (pair<txout_to_key, uint64_t> &outp: txd.output_pub_keys)
    {

        // get the tx output public key
        // that normally would be generated for us,
        // if someone had sent us some xmr.
        public_key tx_pubkey;

        derive_public_key(derivation,
                          output_idx,
                          address_info.address.m_spend_public_key,
                          tx_pubkey);

        // check if generated public key matches the current output's key
        bool mine_output = (outp.first.key == tx_pubkey);
        bool with_additional = false;
        if (!mine_output && txd.additional_pks.size() == txd.output_pub_keys.size())
        {
            derive_public_key(additional_derivations[output_idx],
                              output_idx,
                              address_info.address.m_spend_public_key,
                              tx_pubkey);
            mine_output = (outp.first.key == tx_pubkey);
            with_additional = true;
        }

        // if mine output has RingCT, i.e., tx version is 2
        if (mine_output && tx.version >= cryptonote::txversion::v2)
        {
            // cointbase txs have amounts in plain sight.
            // so use amount from ringct, only for non-coinbase txs
            if (!is_coinbase(tx))
            {

                // initialize with regular amount
                uint64_t rct_amount = money_transfered[output_idx];

                bool r;

                r = decode_ringct(tx.rct_signatures,
                                  with_additional ? additional_derivations[output_idx] : derivation,
                                  output_idx,
                                  tx.rct_signatures.ecdhInfo[output_idx].mask,
                                  rct_amount);

                if (!r)
                {
                    cerr << "\nshow_my_outputs: Cant decode ringCT! " << endl;
                }

                outp.second         = rct_amount;
                money_transfered[output_idx] = rct_amount;

            } // if (!is_coinbase(tx))

        }  // if (mine_output && tx.version == 2)

        j_outptus.push_back(json {
                {"output_pubkey", pod_to_hex(outp.first.key)},
                {"amount"       , outp.second},
                {"match"        , mine_output},
                {"output_idx"   , output_idx},
        });

        ++output_idx;

    } // for (pair<txout_to_key, uint64_t>& outp: txd.output_pub_keys)

    // return parsed values. can be use to double
    // check if submited data in the request
    // matches to what was used to produce response.
    j_data["tx_hash"]  = pod_to_hex(txd.hash);
    j_data["address"]  = REMOVE_HASH_BRACKETS(xmreg::print_address(address_info, nettype));
    j_data["viewkey"]  = pod_to_hex(unwrap(unwrap(prv_view_key)));
    j_data["tx_prove"] = tx_prove;

    j_response["status"] = "success";

    return j_response;
}



json
json_outputsblocks(string _limit,
                   string address_str,
                   string viewkey_str,
                   bool in_mempool_aswell = false)
{
    boost::trim(_limit);
    boost::trim(address_str);
    boost::trim(viewkey_str);

    json j_response {
            {"status", "fail"},
            {"data",   json {}}
    };

    json& j_data = j_response["data"];

    uint64_t no_of_last_blocks {3};

    try
    {
        no_of_last_blocks = boost::lexical_cast<uint64_t>(_limit);
    }
    catch (const boost::bad_lexical_cast &e)
    {
        j_data["title"] = fmt::format(
                "Cant parse page and/or limit numbers: {:s}", _limit);
        return j_response;
    }

    // maxium five last blocks
    no_of_last_blocks = std::min<uint64_t>(no_of_last_blocks, 5ul);

    if (address_str.empty())
    {
        j_response["status"]  = "error";
        j_response["message"] = "Arqma address not provided";
        return j_response;
    }

    if (viewkey_str.empty())
    {
        j_response["status"]  = "error";
        j_response["message"] = "Viewkey not provided";
        return j_response;
    }

    // parse string representing given monero address
    address_parse_info address_info;

    if (!xmreg::parse_str_address(address_str, address_info, nettype))
    {
        j_response["status"]  = "error";
        j_response["message"] = "Cant parse Arqma address: " + address_str;
        return j_response;

    }

    // parse string representing given private key
    crypto::secret_key prv_view_key;

    if (!xmreg::parse_str_secret_key(viewkey_str, prv_view_key))
    {
        j_response["status"]  = "error";
        j_response["message"] = "Cant parse view key: "
                                + viewkey_str;
        return j_response;
    }

    string error_msg;

    j_data["outputs"] = json::array();
    json& j_outptus   = j_data["outputs"];


    if (in_mempool_aswell)
    {
        // first check if there is something for us in the mempool
        // get mempool tx from mempoolstatus thread
        vector<MempoolStatus::mempool_tx> mempool_txs
                = MempoolStatus::get_mempool_txs();

        uint64_t no_mempool_txs = mempool_txs.size();

        // need to use vector<transactions>,
        // not vector<MempoolStatus::mempool_tx>
        vector<transaction> tmp_vector;
        tmp_vector.reserve(no_mempool_txs);

        for (size_t i = 0; i < no_mempool_txs; ++i)
        {
            // get transaction info of the tx in the mempool
            tmp_vector.push_back(std::move(mempool_txs.at(i).tx));
        }

        if (!find_our_outputs(
                address_info.address, prv_view_key,
                0 /* block_no */, true /*is mempool*/,
                tmp_vector.cbegin(), tmp_vector.cend(),
                j_outptus /* found outputs are pushed to this*/,
                error_msg))
        {
            j_response["status"] = "error";
            j_response["message"] = error_msg;
            return j_response;
        }

    } // if (in_mempool_aswell)


    // and now serach for outputs in last few blocks in the blockchain

    uint64_t height = core_storage->get_current_blockchain_height();

    // calculate starting and ending block numbers to show
    int64_t start_height = height - no_of_last_blocks;

    // check if start height is not below range
    start_height = start_height < 0 ? 0 : start_height;

    int64_t end_height = start_height + no_of_last_blocks - 1;

    // loop index
    int64_t block_no = end_height;


    // iterate over last no_of_last_blocks of blocks
    while (block_no >= start_height)
    {
        // get block at the given height block_no
        block blk;

        if (!mcore->get_block_by_height(block_no, blk))
        {
            j_response["status"] = "error";
            j_response["message"] = fmt::format("Cant get block: {:d}", block_no);
            return j_response;
        }

        // get transactions in the given block
        vector<cryptonote::transaction> blk_txs{blk.miner_tx};
        vector<crypto::hash> missed_txs;

        if (!core_storage->get_transactions(blk.tx_hashes, blk_txs, missed_txs))
        {
            j_response["status"] = "error";
            j_response["message"] = fmt::format("Cant get transactions in block: {:d}", block_no);
            return j_response;
        }

        (void) missed_txs;

        if (!find_our_outputs(
                address_info.address, prv_view_key,
                block_no, false /*is mempool*/,
                blk_txs.cbegin(), blk_txs.cend(),
                j_outptus /* found outputs are pushed to this*/,
                error_msg))
        {
            j_response["status"] = "error";
            j_response["message"] = error_msg;
            return j_response;
        }

        --block_no;

    }  //  while (block_no >= start_height)

    // return parsed values. can be use to double
    // check if submited data in the request
    // matches to what was used to produce response.
    j_data["address"]  = REMOVE_HASH_BRACKETS(xmreg::print_address(address_info, nettype));
    j_data["viewkey"]  = pod_to_hex(unwrap(unwrap(prv_view_key)));
    j_data["limit"]    = _limit;
    j_data["height"]   = height;
    j_data["mempool"]  = in_mempool_aswell;

    j_response["status"] = "success";

    return j_response;
}

/*
 * Lets use this json api convention for success and error
 * https://labs.omniti.com/labs/jsend
 */
json
json_networkinfo()
{
    json j_response {
            {"status", "fail"},
            {"data",   json {}}
    };

    json& j_data = j_response["data"];

    json j_info;

    // get basic network info
    if (!get_arqma_network_info(j_info))
    {
        j_response["status"]  = "error";
        j_response["message"] = "Cant get Arqma network info";
        return j_response;
    }

    uint64_t fee_estimated {0};

    // get dynamic fee estimate from last 10 blocks
    if (!get_dynamic_per_kb_fee_estimate(fee_estimated))
    {
        j_response["status"]  = "error";
        j_response["message"] = "Cant get dynamic fee esimate";
        return j_response;
    }

    j_info["fee_per_kb"] = fee_estimated;

    j_info["tx_pool_size"]        = MempoolStatus::mempool_no.load();
    j_info["tx_pool_size_kbytes"] = MempoolStatus::mempool_size.load();

    j_data = j_info;

    j_response["status"]  = "success";

    return j_response;
}


/*
* Lets use this json api convention for success and error
* https://labs.omniti.com/labs/jsend
*/
json
json_emission()
{
    json j_response {
            {"status", "fail"},
            {"data",   json {}}
    };

    json& j_data = j_response["data"];

    json j_info;

    // get basic network info
    if (!CurrentBlockchainStatus::is_thread_running())
    {
        j_data["title"] = "Emission monitoring thread not enabled.";
        return j_response;
    }
    else
    {
        CurrentBlockchainStatus::Emission current_values
                = CurrentBlockchainStatus::get_emission();

        string emission_blk_no   = std::to_string(current_values.blk_no - 1);
        string emission_coinbase = arq_amount_to_str(current_values.coinbase, "{:0.9f}");
        string emission_fee      = arq_amount_to_str(current_values.fee, "{:0.9f}", false);

        j_data = json {
                {"blk_no"            , current_values.blk_no - 1},
                {"coinbase"          , current_values.coinbase},
                {"fee"               , current_values.fee},
        };
    }

    j_response["status"]  = "success";

    return j_response;
}


/*
      * Lets use this json api convention for success and error
      * https://labs.omniti.com/labs/jsend
      */
json
json_version()
{
    json j_response {
            {"status", "fail"},
            {"data",   json {}}
    };

    json& j_data = j_response["data"];

    j_data = json {
            {"last_git_commit_hash", string {GIT_COMMIT_HASH}},
            {"last_git_commit_date", string {GIT_COMMIT_DATETIME}},
            {"git_branch_name"     , string {GIT_BRANCH_NAME}},
            {"arqma_version_full"  , string {ARQMA_VERSION_FULL}},
            {"api"                 , ARQMAEXPLORER_RPC_VERSION},
            {"blockchain_height"   , core_storage->get_current_blockchain_height()}
    };

    j_response["status"]  = "success";

    return j_response;
}


private:


string
get_payment_id_as_string(
        tx_details const &txd,
        secret_key const &prv_view_key)
{
    string payment_id;

    // decrypt encrypted payment id, as used in integreated addresses
    crypto::hash8 decrypted_payment_id8 = txd.payment_id8;

    if (decrypted_payment_id8 != null_hash8)
    {
        if (mcore->get_device()->decrypt_payment_id(decrypted_payment_id8, txd.pk, prv_view_key))
        {
            payment_id = pod_to_hex(decrypted_payment_id8);
        }
    }
    else if(txd.payment_id != null_hash)
    {
        payment_id = pod_to_hex(txd.payment_id);
    }

    return payment_id;
}

template <typename Iterator>
bool
find_our_outputs(
        account_public_address const &address,
        secret_key const &prv_view_key,
        uint64_t const &block_no,
        bool const &is_mempool,
        Iterator const &txs_begin,
        Iterator const &txs_end,
        json &j_outptus,
        string &error_msg)
{

    // for each tx, perform output search using provided
    // address and viewkey
    for (auto it = txs_begin; it != txs_end; ++it)
    {
        cryptonote::transaction const &tx = *it;

        tx_details txd = get_tx_details(tx);

        // public transaction key is combined with our viewkey
        // to create, so called, derived key.
        key_derivation derivation;

        if (!generate_key_derivation(txd.pk, prv_view_key, derivation))
        {
            error_msg = "Cant calculate key_derivation";
            return false;
        }

        std::vector<key_derivation> additional_derivations(txd.additional_pks.size());
        for (size_t i = 0; i < txd.additional_pks.size(); ++i)
        {
            if (!generate_key_derivation(txd.additional_pks[i], prv_view_key, additional_derivations[i]))
            {
                error_msg = "Cant calculate key_derivation";
                return false;
            }
        }

        uint64_t output_idx{0};

        std::vector<uint64_t> money_transfered(tx.vout.size(), 0);

        //j_data["outputs"] = json::array();
        //json& j_outptus   = j_data["outputs"];

        for (pair<txout_to_key, uint64_t> &outp: txd.output_pub_keys)
        {

            // get the tx output public key
            // that normally would be generated for us,
            // if someone had sent us some xmr.
            public_key tx_pubkey;

            derive_public_key(derivation,
                              output_idx,
                              address.m_spend_public_key,
                              tx_pubkey);

            // check if generated public key matches the current output's key
            bool mine_output = (outp.first.key == tx_pubkey);
            bool with_additional = false;
            if (!mine_output && txd.additional_pks.size() == txd.output_pub_keys.size())
            {
                derive_public_key(additional_derivations[output_idx],
                                  output_idx,
                                  address.m_spend_public_key,
                                  tx_pubkey);
                mine_output = (outp.first.key == tx_pubkey);
                with_additional = true;
            }

            // if mine output has RingCT, i.e., tx version is 2
            if (mine_output && tx.version >= cryptonote::txversion::v2)
            {
                // cointbase txs have amounts in plain sight.
                // so use amount from ringct, only for non-coinbase txs
                if (!is_coinbase(tx))
                {

                    // initialize with regular amount
                    uint64_t rct_amount = money_transfered[output_idx];

                    bool r {false};

                    rct::key mask = tx.rct_signatures.ecdhInfo[output_idx].mask;

                    r = decode_ringct(tx.rct_signatures,
                                      with_additional ? additional_derivations[output_idx] : derivation,
                                      output_idx,
                                      mask,
                                      rct_amount);

                    if (!r)
                    {
                        error_msg = "Cant decode ringct for tx: "
                                                + pod_to_hex(txd.hash);
                        return false;
                    }

                    outp.second = rct_amount;
                    money_transfered[output_idx] = rct_amount;

                } // if (!is_coinbase(tx))

            }  // if (mine_output && tx.version == 2)

            if (mine_output)
            {
                string payment_id_str = get_payment_id_as_string(txd, prv_view_key);

                j_outptus.push_back(json {
                        {"output_pubkey" , pod_to_hex(outp.first.key)},
                        {"amount"        , outp.second},
                        {"block_no"      , block_no},
                        {"in_mempool"    , is_mempool},
                        {"output_idx"    , output_idx},
                        {"tx_hash"       , pod_to_hex(txd.hash)},
                        {"payment_id"    , payment_id_str}
                });
            }

            ++output_idx;

        } //  for (pair<txout_to_key, uint64_t>& outp: txd.output_pub_keys)

    } // for (auto it = blk_txs.begin(); it != blk_txs.end(); ++it)

    return true;
}

json
get_tx_json(const transaction &tx, const tx_details &txd)
{

    json j_tx {
            {"tx_hash"     , pod_to_hex(txd.hash)},
            {"tx_fee"      , txd.fee},
            {"mixin"       , txd.mixin_no},
            {"tx_size"     , txd.size},
            {"arq_outputs" , txd.arq_outputs},
            {"arq_inputs"  , txd.arq_inputs},
            {"tx_version"  , static_cast<uint64_t>(txd.version)},
            {"rct_type"    , tx.rct_signatures.type},
            {"coinbase"    , is_coinbase(tx)},
            {"mixin"       , txd.mixin_no},
            {"extra"       , txd.get_extra_str()},
            {"payment_id"  , (txd.payment_id  != null_hash  ? pod_to_hex(txd.payment_id)  : "")},
            {"payment_id8" , (txd.payment_id8 != null_hash8 ? pod_to_hex(txd.payment_id8) : "")},
    };

    return j_tx;
}


bool
find_tx(const crypto::hash &tx_hash,
        transaction &tx,
        bool &found_in_mempool,
        uint64_t &tx_timestamp)
{

    found_in_mempool = false;

    if (!mcore->get_tx(tx_hash, tx))
    {
        cerr << "Cant get tx in blockchain: " << tx_hash
             << ". \n Check mempool now" << endl;

        vector<MempoolStatus::mempool_tx> found_txs;

        search_mempool(tx_hash, found_txs);

        if (!found_txs.empty())
        {
            // there should be only one tx found
            tx = found_txs.at(0).tx;
            found_in_mempool = true;
            tx_timestamp = found_txs.at(0).receive_time;
        }
        else
        {
            // tx is nowhere to be found :-(
            return false;
        }
    }

    return true;
}


void
mark_real_mixins_on_timescales(
        const vector<uint64_t> &real_output_indices,
        mstch::map &tx_context)
{
    // mark real mixing in the mixins timescale graph
    mstch::array& mixins_timescales
            = boost::get<mstch::array>(tx_context["timescales"]);

    uint64_t idx {0};

    for (mstch::node &timescale_node: mixins_timescales)
    {

        string& timescale = boost::get<string>(
                boost::get<mstch::map>(timescale_node)["timescale"]
        );

        // claculated number of timescale points
        // due to resolution, no of points might be lower than no of mixins
        size_t no_points = std::count(timescale.begin(), timescale.end(), '*');

        size_t point_to_find = real_output_indices.at(idx);

        // adjust point to find based on total number of points
        if (point_to_find >= no_points)
            point_to_find = no_points  - 1;

        boost::iterator_range<string::iterator> r
                = boost::find_nth(timescale, "*", point_to_find);

        *(r.begin()) = 'R';

        ++idx;
    }
}

std::string extract_sn_pubkey(const std::vector<uint8_t> &tx_extra)
{
  crypto::public_key snode_key;
  if (get_service_node_pubkey_from_tx_extra(tx_extra, snode_key))
  {
    return pod_to_hex(snode_key);
  }
  else
  {
    return "<pubkey parsing error>";
  }
}

inline uint64_t get_amount_from_stake(const cryptonote::transaction &tx, const cryptonote::account_public_address &contributor)
{
  uint64_t amount = 0;
  crypto::secret_key tx_key;
  crypto::key_derivation derivation;
  if (cryptonote::get_tx_secret_key_from_tx_extra(tx.extra, tx_key) &&
      generate_key_derivation(contributor.m_view_public_key, tx_key, derivation) &&
      !tx.vout.empty() && tx.vout.back().target.type() == typeid(cryptonote::txout_to_key))
  {
    hw::device &hwdev = hw::get_device("default");
    size_t tx_offset = tx.vout.size() - 1;
    rct::key mask;
    crypto::secret_key scalar1;
    hwdev.derivation_to_scalar(derivation, tx_offset, scalar1);
    try
    {
      switch (tx.rct_signatures.type)
      {
        case rct::RCTTypeSimple:
        case rct::RCTTypeSimpleBulletproof:
        case rct::RCTTypeBulletproof:
        case rct::RCTTypeBulletproof2:
          amount = rct::decodeRctSimple(tx.rct_signatures, rct::sk2rct(scalar1), tx_offset, mask, hwdev);
          break;
        case rct::RCTTypeFull:
        case rct::RCTTypeFullBulletproof:
          amount = rct::decodeRct(tx.rct_signatures, rct::sk2rct(scalar1), tx_offset, mask, hwdev);
          break;
        default:
          break;
      }
    }
    catch (const std::exception &e) { }
  }
  return amount;
}

mstch::map
construct_tx_context(transaction tx, uint16_t with_ring_signatures = 0)
{
    tx_details txd = get_tx_details(tx);

    const crypto::hash &tx_hash = txd.hash;

    string tx_hash_str = pod_to_hex(tx_hash);

    uint64_t tx_blk_height {0};

    bool tx_blk_found {false};

    bool detailed_view {enable_mixins_details || static_cast<bool>(with_ring_signatures)};

    if (core_storage->have_tx(tx_hash))
    {
        // currently get_tx_block_height seems to return a block hight
        // +1. Before it was not like this.
        tx_blk_height = core_storage->get_db().get_tx_block_height(tx_hash);
        tx_blk_found = true;
    }

    // get block cointaining this tx
    block blk;

    if (tx_blk_found && !mcore->get_block_by_height(tx_blk_height, blk))
    {
        cerr << "Cant get block: " << tx_blk_height << endl;
    }

    string tx_blk_height_str {"N/A"};

    // tx age
    pair<string, string> age;

    string blk_timestamp {"N/A"};

    if (tx_blk_found)
    {
        // calculate difference between tx and server timestamps
        age = get_age(server_timestamp, blk.timestamp, FULL_AGE_FORMAT);

        blk_timestamp = xmreg::timestamp_to_str_gm(blk.timestamp);

        tx_blk_height_str = std::to_string(tx_blk_height);
    }

    // payments id. both normal and encrypted (payment_id8)
    string pid_str   = pod_to_hex(txd.payment_id);
    string pid8_str  = pod_to_hex(txd.payment_id8);


    string tx_json = obj_to_json_str(tx);

    // use this regex to remove all non friendly characters in payment_id_as_ascii string
    static std::regex e {"[^a-zA-Z0-9 ./\\\\!]"};

    double tx_size = static_cast<double>(txd.size) / 1024.0;

    double payed_for_kB = ARQ_AMOUNT(txd.fee) / tx_size;

    // initalise page tempate map with basic info about blockchain
    mstch::map context {
            {"testnet"               , testnet},
            {"stagenet"              , stagenet},
            {"tx_hash"               , tx_hash_str},
            {"tx_prefix_hash"        , string{}},
            {"tx_pub_key"            , pod_to_hex(txd.pk)},
            {"blk_height"            , tx_blk_height_str},
            {"tx_blk_height"         , tx_blk_height},
            {"tx_size"               , fmt::format("{:0.4f}", tx_size)},
            {"tx_fee"                , xmreg::arq_amount_to_str(txd.fee, "{:0.9f}", false)},
            {"tx_fee_nano"           , xmreg::arq_amount_to_str(txd.fee*1e9, "{:0.9f}", false)},
            {"payed_for_kB"          , fmt::format("{:0.9f}", payed_for_kB)},
            {"tx_version"            , static_cast<uint64_t>(txd.version)},
            {"blk_timestamp"         , blk_timestamp},
            {"blk_timestamp_uint"    , blk.timestamp},
            {"delta_time"            , age.first},
            {"inputs_no"             , static_cast<uint64_t>(txd.input_key_imgs.size())},
            {"has_inputs"            , !txd.input_key_imgs.empty()},
            {"outputs_no"            , static_cast<uint64_t>(txd.output_pub_keys.size())},
            {"has_payment_id"        , txd.payment_id  != null_hash},
            {"has_payment_id8"       , txd.payment_id8 != null_hash8},
            {"confirmations"         , txd.no_confirmations},
            {"payment_id"            , pid_str},
            {"payment_id_as_ascii"   , remove_bad_chars(txd.payment_id_as_ascii)},
            {"payment_id8"           , pid8_str},
            {"extra"                 , txd.get_extra_str()},
            {"with_ring_signatures"  , static_cast<bool>(with_ring_signatures)},
            {"tx_json"               , tx_json},
            {"is_ringct"             , tx.version >= cryptonote::txversion::v2},
            {"tx_type"               , string(cryptonote::transaction::type_to_string(tx.tx_type))},
            {"rct_type"              , tx.rct_signatures.type},
            {"has_error"             , false},
            {"error_msg"             , string("")},
            {"have_raw_tx"           , false},
            {"show_more_details_link", true},
            {"construction_time"     , string {}},
    };

    add_tx_metadata(context, tx, true);

    // append tx_json as in raw format to html
    context["tx_json_raw"] = mstch::lambda{[=](const std::string& text) -> mstch::node {
        return tx_json;
    }};

    // append additional public tx keys, if there are any, to the html context

    string add_tx_pub_keys;

    for (auto const &apk: txd.additional_pks)
        add_tx_pub_keys += pod_to_hex(apk) + ";";

    context["add_tx_pub_keys"] = add_tx_pub_keys;

    string server_time_str = xmreg::timestamp_to_str_gm(server_timestamp, "%F");

    mstch::array inputs = mstch::array{};

    uint64_t input_idx {0};

    uint64_t inputs_arq_sum {0};

    // ringct inputs can be mixture of known amounts (when old outputs)
    // are spent, and unknown umounts (makrked in explorer by '?') when
    // ringct outputs are spent. thus we totalling input amounts
    // in such case, we need to show sum of known umounts, and
    // indicate that this is minium sum, as we dont know the unknown
    // umounts.
    bool have_any_unknown_amount {false};

    uint64_t max_no_of_inputs_to_show {10};

    // if a tx has more inputs than max_no_of_inputs_to_show,
    // we only show 10 first.
    bool show_part_of_inputs = (txd.input_key_imgs.size() > max_no_of_inputs_to_show);

    // but if we show all details, i.e.,
    // the show all inputs, regardless of their number
    if (detailed_view)
    {
        show_part_of_inputs = false;
    }

    vector<vector<uint64_t>> mixin_timestamp_groups;

    // make timescale maps for mixins in input
    for (const txin_to_key &in_key: txd.input_key_imgs)
    {
        if (show_part_of_inputs && (input_idx > max_no_of_inputs_to_show))
            break;

        // get absolute offsets of mixins
        std::vector<uint64_t> absolute_offsets
                = cryptonote::relative_output_offsets_to_absolute(
                        in_key.key_offsets);

        // get public keys of outputs used in the mixins that match to the offests
        std::vector<cryptonote::output_data_t> outputs;

        try
        {
            // before proceeding with geting the outputs based on the amount and absolute offset
            // check how many outputs there are for that amount
            // go to next input if a too large offset was found
            if (are_absolute_offsets_good(absolute_offsets, in_key) == false)
                continue;

            // offsets seems good, so try to get the outputs for the amount and
            // offsets given
            core_storage->get_db().get_output_key(epee::span<const uint64_t>(&in_key.amount, 1),
                                                  absolute_offsets,
                                                  outputs);
        }
        catch (const std::exception &e)
        {
            string out_msg = fmt::format(
                    "Outputs with amount {:d} do not exist and indexes ",
                    in_key.amount
            );

            for (auto offset: absolute_offsets)
                out_msg += ", " + to_string(offset);

            out_msg += " don't exist! " + string {e.what()};

            cerr << out_msg << '\n';

            context["has_error"] = true;
            context["error_msg"] = out_msg;

            return context;
        }

        inputs.push_back(mstch::map {
                {"in_key_img"   , pod_to_hex(in_key.k_image)},
                {"amount"       , xmreg::arq_amount_to_str(in_key.amount)},
                {"input_idx"    , fmt::format("{:02d}", input_idx)},
                {"mixins"       , mstch::array{}},
                {"ring_sigs"    , mstch::array{}},
                {"already_spent", false} // placeholder for later
        });

        if (detailed_view)
        {
            boost::get<mstch::map>(inputs.back())["ring_sigs"]
                    = txd.get_ring_sig_for_input(input_idx);
        }


        inputs_arq_sum += in_key.amount;

        if (in_key.amount == 0)
        {
            // if any input has amount equal to zero,
            // it is really an unkown amount
            have_any_unknown_amount = true;
        }

        vector<uint64_t> mixin_timestamps;

        // get reference to mixins array created above
        mstch::array &mixins = boost::get<mstch::array>(
                boost::get<mstch::map>(inputs.back())["mixins"]);

        // mixin counter
        size_t count = 0;

        // for each found output public key find its block to get timestamp
        for (const uint64_t &i: absolute_offsets)
        {
            // get basic information about mixn's output
            cryptonote::output_data_t output_data = outputs.at(count);

            tx_out_index tx_out_idx;

            try
            {
                // get pair pair<crypto::hash, uint64_t> where first is tx hash
                // and second is local index of the output i in that tx
                tx_out_idx = core_storage->get_db()
                        .get_output_tx_and_index(in_key.amount, i);
            }
            catch (const OUTPUT_DNE &e)
            {

                string out_msg = fmt::format(
                        "Output with amount {:d} and index {:d} does not exist!",
                        in_key.amount, i
                );

                cerr << out_msg << endl;

                context["has_error"] = true;
                context["error_msg"] = out_msg;

                return context;
            }


            if (detailed_view)
            {
                // get block of given height, as we want to get its timestamp
                cryptonote::block blk;

                if (!mcore->get_block_by_height(output_data.height, blk))
                {
                    cerr << "- cant get block of height: " << output_data.height << endl;

                    context["has_error"] = true;
                    context["error_msg"] = fmt::format("- cant get block of height: {}",
                                                       output_data.height);
                }

                // get age of mixin relative to server time
                pair<string, string> mixin_age = get_age(server_timestamp,
                                                         blk.timestamp,
                                                         FULL_AGE_FORMAT);
                // get mixin transaction
                transaction mixin_tx;

                if (!mcore->get_tx(tx_out_idx.first, mixin_tx))
                {
                    cerr << "Cant get tx: " << tx_out_idx.first << endl;

                    context["has_error"] = true;
                    context["error_msg"] = fmt::format("Cant get tx: {:s}", tx_out_idx.first);
                }

                // mixin tx details
                tx_details mixin_txd = get_tx_details(mixin_tx, true);

                mixins.push_back(mstch::map {
                        {"mix_blk",        fmt::format("{:08d}", output_data.height)},
                        {"mix_pub_key",    pod_to_hex(output_data.pubkey)},
                        {"mix_tx_hash",    pod_to_hex(tx_out_idx.first)},
                        {"mix_out_indx",   tx_out_idx.second},
                        {"mix_timestamp",  xmreg::timestamp_to_str_gm(blk.timestamp)},
                        {"mix_age",        mixin_age.first},
                        {"mix_mixin_no",   mixin_txd.mixin_no},
                        {"mix_inputs_no",  static_cast<uint64_t>(mixin_txd.input_key_imgs.size())},
                        {"mix_outputs_no", static_cast<uint64_t>(mixin_txd.output_pub_keys.size())},
                        {"mix_age_format", mixin_age.second},
                        {"mix_idx",        fmt::format("{:02d}", count)},
                        {"mix_is_it_real", false}, // a placeholder for future
                });

                // get mixin timestamp from its orginal block
                mixin_timestamps.push_back(blk.timestamp);
            }
            else //  if (detailed_view)
            {
                mixins.push_back(mstch::map {
                        {"mix_blk",        fmt::format("{:08d}", output_data.height)},
                        {"mix_pub_key",    pod_to_hex(output_data.pubkey)},
                        {"mix_idx",        fmt::format("{:02d}", count)},
                        {"mix_is_it_real", false}, // a placeholder for future
                });

            } // else  if (enable_mixins_details)

            ++count;

        } // for (const uint64_t &i: absolute_offsets)

        mixin_timestamp_groups.push_back(mixin_timestamps);

        input_idx++;

    } // for (const txin_to_key& in_key: txd.input_key_imgs)



    if (detailed_view)
    {
        uint64_t min_mix_timestamp {0};
        uint64_t max_mix_timestamp {0};

        pair<mstch::array, double> mixins_timescales
                = construct_mstch_mixin_timescales(
                        mixin_timestamp_groups,
                        min_mix_timestamp,
                        max_mix_timestamp
                );


        context["min_mix_time"]     = xmreg::timestamp_to_str_gm(min_mix_timestamp);
        context["max_mix_time"]     = xmreg::timestamp_to_str_gm(max_mix_timestamp);

        context.emplace("timescales", mixins_timescales.first);


        context["timescales_scale"] = fmt::format("{:0.2f}",
                                                  mixins_timescales.second / 3600.0 / 24.0); // in days

        context["tx_prefix_hash"] = pod_to_hex(get_transaction_prefix_hash(tx));

    }


    context["have_any_unknown_amount"]  = have_any_unknown_amount;
    context["inputs_arq_sum_not_zero"]  = (inputs_arq_sum > 0);
    context["inputs_arq_sum"]           = xmreg::arq_amount_to_str(inputs_arq_sum);
    context["server_time"]              = server_time_str;
    context["enable_mixins_details"]    = detailed_view;
    context["enable_as_hex"]            = enable_as_hex;
    context["show_part_of_inputs"]      = show_part_of_inputs;
    context["max_no_of_inputs_to_show"] = max_no_of_inputs_to_show;


    context.emplace("inputs", inputs);

    // get indices of outputs in amounts tables
    vector<uint64_t> out_amount_indices;

    try
    {

        uint64_t tx_index;

        if (core_storage->get_db().tx_exists(txd.hash, tx_index))
        {
            out_amount_indices = core_storage->get_db()
                    .get_tx_amount_output_indices(tx_index).front();
        }
        else
        {
            cerr << "get_tx_outputs_gindexs failed to find transaction with id = " << txd.hash;
        }

    }
    catch(const exception &e)
    {
        cerr << e.what() << endl;
    }

    uint64_t output_idx {0};

    mstch::array outputs;

    uint64_t outputs_arq_sum {0};

    for (pair<txout_to_key, uint64_t> &outp: txd.output_pub_keys)
    {

        // total number of ouputs in the blockchain for this amount
        uint64_t num_outputs_amount = core_storage->get_db()
                .get_num_outputs(outp.second);

        string out_amount_index_str {"N/A"};

        // outputs in tx in them mempool dont have yet global indices
        // thus for them, we print N/A
        if (!out_amount_indices.empty())
        {
            out_amount_index_str
                    = std::to_string(out_amount_indices.at(output_idx));
        }

        outputs_arq_sum += outp.second;

        outputs.push_back(mstch::map {
                {"out_pub_key"           , pod_to_hex(outp.first.key)},
                {"amount"                , xmreg::arq_amount_to_str(outp.second)},
                {"amount_idx"            , out_amount_index_str},
                {"num_outputs"           , num_outputs_amount},
                {"unformated_output_idx" , output_idx},
                {"output_idx"            , fmt::format("{:02d}", output_idx++)}
        });

    } //  for (pair<txout_to_key, uint64_t>& outp: txd.output_pub_keys)

    context["outputs_arq_sum"] = xmreg::arq_amount_to_str(outputs_arq_sum);

    context.emplace("outputs", outputs);


    return context;
}

pair<mstch::array, double>
construct_mstch_mixin_timescales(
        const vector<vector<uint64_t>> &mixin_timestamp_groups,
        uint64_t &min_mix_timestamp,
        uint64_t &max_mix_timestamp
)
{
    mstch::array mixins_timescales;

    double timescale_scale {0.0};     // size of one '_' in days

    // initialize with some large and some numbers
    min_mix_timestamp = server_timestamp*2L;
    max_mix_timestamp = 0;

    // find min and maximum timestamps
    for (const vector<uint64_t> &mixn_timestamps : mixin_timestamp_groups)
    {

        uint64_t min_found = *min_element(mixn_timestamps.begin(), mixn_timestamps.end());
        uint64_t max_found = *max_element(mixn_timestamps.begin(), mixn_timestamps.end());

        if (min_found < min_mix_timestamp)
            min_mix_timestamp = min_found;

        if (max_found > max_mix_timestamp)
            max_mix_timestamp = max_found;
    }


    min_mix_timestamp -= 3600;
    max_mix_timestamp += 3600;

    // make timescale maps for mixins in input with adjusted range
    for (auto &mixn_timestamps : mixin_timestamp_groups)
    {
        // get mixins in time scale for visual representation
        pair<string, double> mixin_times_scale = xmreg::timestamps_time_scale(
                mixn_timestamps,
                max_mix_timestamp,
                170,
                min_mix_timestamp);

        // save resolution of mixin timescales
        timescale_scale = mixin_times_scale.second;

        // save the string timescales for later to show
        mixins_timescales.push_back(mstch::map {
                {"timescale", mixin_times_scale.first}
        });
    }

    return make_pair(mixins_timescales, timescale_scale);
}


tx_details
get_tx_details(const transaction &tx,
               bool coinbase = false,
               uint64_t blk_height = 0,
               uint64_t bc_height = 0)
{
    tx_details txd;

    // get tx hash
    txd.hash = get_transaction_hash(tx);

    // get tx public key from extra
    // this check if there are two public keys
    // due to previous bug with sining txs:
    // https://github.com/monero-project/monero/pull/1358/commits/7abfc5474c0f86e16c405f154570310468b635c2
    txd.pk = xmreg::get_tx_pub_key_from_received_outs(tx);
    txd.additional_pks = cryptonote::get_additional_tx_pub_keys_from_extra(tx);


    // sum xmr in inputs and ouputs in the given tx
    const array<uint64_t, 4> &sum_data = summary_of_in_out_rct(
            tx, txd.output_pub_keys, txd.input_key_imgs);

    txd.arq_outputs       = sum_data[0];
    txd.arq_inputs        = sum_data[1];
    txd.mixin_no          = sum_data[2];
    txd.num_nonrct_inputs = sum_data[3];

    txd.fee = 0;

    if (!coinbase && tx.vin.size() > 0)
    {
        // check if not miner tx
        // i.e., for blocks without any user transactions
        if (tx.vin.at(0).type() != typeid(txin_gen))
        {
            // get tx fee
            txd.fee = get_tx_fee(tx);
        }
    }

    txd.pID = '-'; // no payment ID

    get_payment_id(tx, txd.payment_id, txd.payment_id8);

    // get tx size in bytes
    txd.size = get_object_blobsize(tx);

    txd.extra = tx.extra;

    if (txd.payment_id != null_hash)
    {
        txd.payment_id_as_ascii = std::string(txd.payment_id.data, crypto::HASH_SIZE);
        txd.pID = 'l'; // legacy payment id
    }
    else if (txd.payment_id8 != null_hash8)
    {
        txd.pID = 'e'; // encrypted payment id
    }
    else if (txd.additional_pks.empty() == false)
    {
        // if multioutput tx have additional public keys,
        // mark it so that it represents that it has at least
        // one sub-address
        txd.pID = 's';
    }

    // get tx signatures for each input
    txd.signatures = tx.signatures;

    // get tx version
    txd.version = static_cast<std::underlying_type<cryptonote::txversion>::type>(tx.version);

    // get unlock time
    txd.unlock_time = tx.unlock_time;

    txd.no_confirmations = 0;

    if (blk_height == 0 && core_storage->have_tx(txd.hash))
    {
        // if blk_height is zero then search for tx block in
        // the blockchain. but since often block height is know a priory
        // this is not needed

        txd.blk_height = core_storage->get_db().get_tx_block_height(txd.hash);

        // get the current blockchain height. Just to check
        uint64_t bc_height = core_storage->get_current_blockchain_height();

        txd.no_confirmations = bc_height - (txd.blk_height);
    }
    else
    {
        // if we know blk_height, and current blockchan height
        // just use it to get no_confirmations.

        txd.no_confirmations = bc_height - (blk_height);
    }

    return txd;
}

void
clean_post_data(string &raw_tx_data)
{
    // remove white characters
    boost::trim(raw_tx_data);
    boost::erase_all(raw_tx_data, "\r\n");
    boost::erase_all(raw_tx_data, "\n");

    // remove header and footer from base64 data
    // produced by certutil.exe in windows
    boost::erase_all(raw_tx_data, "-----BEGIN CERTIFICATE-----");
    boost::erase_all(raw_tx_data, "-----END CERTIFICATE-----");
}


bool
find_tx_for_json(
        string const &tx_hash_str,
        transaction &tx,
        bool &found_in_mempool,
        uint64_t &tx_timestamp,
        string &error_message)
{
    // parse tx hash string to hash object
    crypto::hash tx_hash;

    if (!xmreg::parse_str_secret_key(tx_hash_str, tx_hash))
    {
        error_message = fmt::format("Cant parse tx hash: {:s}", tx_hash_str);
        return false;
    }

    if (!find_tx(tx_hash, tx, found_in_mempool, tx_timestamp))
    {
        error_message = fmt::format("Cant find tx hash: {:s}", tx_hash_str);
        return false;
    }

    return true;
}

bool
search_mempool(crypto::hash tx_hash,
               vector<MempoolStatus::mempool_tx> &found_txs)
{
    // if tx_hash == null_hash then this method
    // will just return the vector containing all
    // txs in mempool



    // get mempool tx from mempoolstatus thread
    vector<MempoolStatus::mempool_tx> mempool_txs
            = MempoolStatus::get_mempool_txs();

    // if dont have tx_blob member, construct tx
    // from json obtained from the rpc call

    for (size_t i = 0; i < mempool_txs.size(); ++i)
    {
        // get transaction info of the tx in the mempool
        const MempoolStatus::mempool_tx &mempool_tx = mempool_txs.at(i);

        if (tx_hash == mempool_tx.tx_hash || tx_hash == null_hash)
        {
            found_txs.push_back(mempool_tx);

            if (tx_hash != null_hash)
                break;
        }

    } // for (size_t i = 0; i < mempool_txs.size(); ++i)

    return true;
}

pair<string, string>
get_age(uint64_t timestamp1, uint64_t timestamp2, bool full_format = 0)
{

    pair<string, string> age_pair;

    // calculate difference between server and block timestamps
    array<size_t, 5> delta_time = timestamp_difference(
            timestamp1, timestamp2);

    // default format for age
    string age_str = fmt::format("{:02d}:{:02d}:{:02d}",
                                 delta_time[2], delta_time[3],
                                 delta_time[4]);

    string age_format {"[h:m:s]"};

    // if have days or years, change age format
    if (delta_time[0] > 0 || full_format == true)
    {
        age_str = fmt::format("{:02d}:{:03d}:{:02d}:{:02d}:{:02d}",
                              delta_time[0], delta_time[1], delta_time[2],
                              delta_time[3], delta_time[4]);

        age_format = string("[y:d:h:m:s]");
    }
    else if (delta_time[1] > 0)
    {
        age_str = fmt::format("{:02d}:{:02d}:{:02d}:{:02d}",
                              delta_time[1], delta_time[2],
                              delta_time[3], delta_time[4]);

        age_format = string("[d:h:m:s]");
    }

    age_pair.first  = age_str;
    age_pair.second = age_format;

    return age_pair;
}


string
get_full_page(const string& middle)
{
    return template_file["header"]
           + middle
           + template_file["footer"];
}

bool
get_arqma_network_info(json& j_info)
{
    MempoolStatus::network_info local_copy_network_info
        = MempoolStatus::current_network_info;

    j_info = json {
       {"status"                    , local_copy_network_info.current},
       {"current"                   , local_copy_network_info.current},
       {"height"                    , local_copy_network_info.height},
       {"target_height"             , local_copy_network_info.target_height},
       {"difficulty"                , local_copy_network_info.difficulty},
       {"target"                    , local_copy_network_info.target},
       {"hash_rate"                 , local_copy_network_info.hash_rate},
       {"tx_count"                  , local_copy_network_info.tx_count},
       {"tx_pool_size"              , local_copy_network_info.tx_pool_size},
       {"alt_blocks_count"          , local_copy_network_info.alt_blocks_count},
       {"outgoing_connections_count", local_copy_network_info.outgoing_connections_count},
       {"incoming_connections_count", local_copy_network_info.incoming_connections_count},
       {"white_peerlist_size"       , local_copy_network_info.white_peerlist_size},
       {"grey_peerlist_size"        , local_copy_network_info.grey_peerlist_size},
       {"testnet"                   , local_copy_network_info.nettype == cryptonote::network_type::TESTNET},
       {"stagenet"                  , local_copy_network_info.nettype == cryptonote::network_type::STAGENET},
       {"top_block_hash"            , pod_to_hex(local_copy_network_info.top_block_hash)},
       {"cumulative_difficulty"     , local_copy_network_info.cumulative_difficulty},
       {"block_size_limit"          , local_copy_network_info.block_size_limit},
       {"block_size_median"         , local_copy_network_info.block_size_median},
       {"start_time"                , local_copy_network_info.start_time},
       {"fee_per_kb"                , local_copy_network_info.fee_per_kb},
       {"current_hf_version"        , local_copy_network_info.current_hf_version}
    };

    return local_copy_network_info.current;
}

bool
get_dynamic_per_kb_fee_estimate(uint64_t &fee_estimated)
{

    string error_msg;

    if (!rpc.get_dynamic_per_kb_fee_estimate(
            FEE_ESTIMATE_GRACE_BLOCKS,
            fee_estimated, error_msg))
    {
        cerr << "rpc.get_dynamic_per_kb_fee_estimate failed" << endl;
        return false;
    }

    (void) error_msg;

    return true;
}

bool
are_absolute_offsets_good(
        std::vector<uint64_t>const &absolute_offsets,
        txin_to_key const &in_key)
{
    // before proceeding with geting the outputs based on the amount and absolute offset
    // check how many outputs there are for that amount
    uint64_t no_outputs = core_storage->get_db().get_num_outputs(in_key.amount);

    bool offset_too_large {false};

    int offset_idx {-1};

    for (auto o: absolute_offsets)
    {
        offset_idx++;

        if (o >= no_outputs)
        {
            offset_too_large = true;
            cerr << "Absolute offset (" << o << ") of an output in a key image "
                 << pod_to_hex(in_key.k_image)
                 << " (ring member no: " << offset_idx << ") "
                 << "for amount "  << in_key.amount
                 << " is too large. There are only "
                 << no_outputs << " such outputs!\n";
            continue;
        }
    }

    return !offset_too_large;
}

string
get_footer()
{
    // set last git commit date based on
    // autogenrated version.h during compilation
    static const mstch::map footer_context {
            {"last_git_commit_hash", string {GIT_COMMIT_HASH}},
            {"last_git_commit_date", string {GIT_COMMIT_DATETIME}},
            {"git_branch_name"     , string {GIT_BRANCH_NAME}},
            {"arqma_version_full"  , string {ARQMA_VERSION_FULL}},
            {"api"                 , std::to_string(ARQMAEXPLORER_RPC_VERSION_MAJOR)
                                     + "."
                                     + std::to_string(ARQMAEXPLORER_RPC_VERSION_MINOR)},
    };

    string footer_html = mstch::render(xmreg::read(TMPL_FOOTER), footer_context);

    return footer_html;
}

void
add_css_style(mstch::map& context)
{
    // add_css_style goes to every subpage so here we mark

    context["css_styles"] = mstch::lambda{[&](const std::string& text) -> mstch::node {
        return template_file["css_styles"];
    }};
}

bool
get_tx(string const &tx_hash_str,
       transaction &tx,
       crypto::hash &tx_hash)
{
    if (!epee::string_tools::hex_to_pod(tx_hash_str, tx_hash))
    {
        string msg = fmt::format("Cant parse {:s} as tx hash!", tx_hash_str);
        cerr << msg << endl;
        return false;
    }

    // get transaction

    if (!mcore->get_tx(tx_hash, tx))
    {
        cerr << "Cant get tx in blockchain: " << tx_hash
             << ". \n Check mempool now\n";

        vector<MempoolStatus::mempool_tx> found_txs;

        search_mempool(tx_hash, found_txs);

        if (found_txs.empty())
        {
            // tx is nowhere to be found :-(
            return false;
        }

        tx = found_txs.at(0).tx;
    }

    return true;
}

public:
    string get_css() { return template_file["css_styles"]; }
    string get_manifest() { return template_file["site_manifest"]; }
};
}


#endif //CROWXMR_PAGE_H
