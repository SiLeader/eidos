//
// Created by MiyaMoto on 2021/02/13.
//

#pragma once

#include <boost/log/trivial.hpp>
#include <random>

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#include <libnuraft/nuraft.hxx>
#pragma GCC diagnostic warning "-Wimplicit-int-conversion"
#pragma GCC diagnostic warning "-Wunused-parameter"

#include "storage_base.hpp"

namespace eidos::storage {

namespace detail {

inline void EncodeMessage(nuraft::buffer_serializer& bs, const BytesMessage& message) {
  const auto& mb = message.bytes();
  bs.put_bytes(static_cast<const void*>(mb.data()), mb.size());
}

class Logger : public nuraft::logger {
 public:
  void trace(const std::string& log_line) { BOOST_LOG_TRIVIAL(trace) << log_line; }
  void debug(const std::string& log_line) override { BOOST_LOG_TRIVIAL(debug) << log_line; }
  void info(const std::string& log_line) override { BOOST_LOG_TRIVIAL(info) << log_line; }
  void warn(const std::string& log_line) override { BOOST_LOG_TRIVIAL(warning) << log_line; }
  void err(const std::string& log_line) override { BOOST_LOG_TRIVIAL(error) << log_line; }
  void fatal(const std::string& log_line) { BOOST_LOG_TRIVIAL(fatal) << log_line; }

 public:
  void put_details(int level, const char*, const char*, size_t, const std::string& log_line) override {
    std::stringstream ss;
    ss << log_line;
    switch (level) {
      case 1:
        fatal(ss.str());
        break;
      case 2:
        err(ss.str());
        break;
      case 3:
        warn(ss.str());
        break;
      case 4:
        info(ss.str());
        break;
      case 5:
        debug(ss.str());
        break;
      case 6:
        trace(ss.str());
        break;
      default:
        err("unknown level");
    }
  }
};

class InMemoryLogStore : public nuraft::log_store {
 public:
  InMemoryLogStore() : start_idx_(1) {
    // Dummy entry for index 0.
    nuraft::ptr<nuraft::buffer> buf = nuraft::buffer::alloc(2);
    logs_[0] = nuraft::cs_new<nuraft::log_entry>(0, buf);
  }

  ~InMemoryLogStore() override = default;

  __nocopy__(InMemoryLogStore);

  nuraft::ulong next_slot() const override {
    std::lock_guard<std::mutex> l(logs_lock_);
    // Exclude the dummy entry.
    return start_idx_ + logs_.size() - 1;
  }

  nuraft::ulong start_index() const override { return start_idx_; }

  nuraft::ptr<nuraft::log_entry> last_entry() const override {
    nuraft::ulong next_idx = next_slot();
    std::lock_guard<std::mutex> l(logs_lock_);
    auto entry = logs_.find(next_idx - 1);
    if (entry == logs_.end()) {
      entry = logs_.find(0);
    }

    return make_clone(entry->second);
  }

  nuraft::ulong append(nuraft::ptr<nuraft::log_entry>& entry) override {
    nuraft::ptr<nuraft::log_entry> clone = make_clone(entry);

    std::lock_guard<std::mutex> l(logs_lock_);
    size_t idx = start_idx_ + logs_.size() - 1;
    logs_[idx] = clone;
    return idx;
  }

  void write_at(nuraft::ulong index, nuraft::ptr<nuraft::log_entry>& entry) override {
    nuraft::ptr<nuraft::log_entry> clone = make_clone(entry);

    // Discard all logs equal to or greater than `index.
    std::lock_guard<std::mutex> l(logs_lock_);
    auto itr = logs_.lower_bound(index);
    while (itr != logs_.end()) {
      itr = logs_.erase(itr);
    }
    logs_[index] = clone;
  }

  nuraft::ptr<std::vector<nuraft::ptr<nuraft::log_entry>>> log_entries(nuraft::ulong start,
                                                                       nuraft::ulong end) override {
    nuraft::ptr<std::vector<nuraft::ptr<nuraft::log_entry>>> ret =
        nuraft::cs_new<std::vector<nuraft::ptr<nuraft::log_entry>>>();

    ret->resize(end - start);
    nuraft::ulong cc = 0;
    for (nuraft::ulong ii = start; ii < end; ++ii) {
      nuraft::ptr<nuraft::log_entry> src;
      {
        std::lock_guard<std::mutex> l(logs_lock_);
        auto entry = logs_.find(ii);
        if (entry == logs_.end()) {
          assert(0);
        }
        src = entry->second;
      }
      (*ret)[cc++] = make_clone(src);
    }
    return ret;
  }

  nuraft::ptr<std::vector<nuraft::ptr<nuraft::log_entry>>> log_entries_ext(
      nuraft::ulong start, nuraft::ulong end, nuraft::int64 batch_size_hint_in_bytes) override {
    nuraft::ptr<std::vector<nuraft::ptr<nuraft::log_entry>>> ret =
        nuraft::cs_new<std::vector<nuraft::ptr<nuraft::log_entry>>>();

    if (batch_size_hint_in_bytes < 0) {
      return ret;
    }

    size_t accum_size = 0;
    for (nuraft::ulong ii = start; ii < end; ++ii) {
      nuraft::ptr<nuraft::log_entry> src;
      {
        std::lock_guard<std::mutex> l(logs_lock_);
        auto entry = logs_.find(ii);
        if (entry == logs_.end()) {
          assert(0);
        }
        src = entry->second;
      }
      ret->push_back(make_clone(src));
      accum_size += src->get_buf().size();
      if (batch_size_hint_in_bytes && accum_size >= (nuraft::ulong)batch_size_hint_in_bytes) break;
    }
    return ret;
  }

  nuraft::ptr<nuraft::log_entry> entry_at(nuraft::ulong index) override {
    nuraft::ptr<nuraft::log_entry> src;
    {
      std::lock_guard lg(logs_lock_);
      auto entry = logs_.find(index);
      if (entry == logs_.end()) {
        entry = logs_.find(0);
      }
      src = entry->second;
    }
    return make_clone(src);
  }

  nuraft::ulong term_at(nuraft::ulong index) override {
    nuraft::ulong term;
    {
      std::lock_guard lg(logs_lock_);
      auto entry = logs_.find(index);
      if (entry == logs_.end()) {
        entry = logs_.find(0);
      }
      term = entry->second->get_term();
    }
    return term;
  }

  nuraft::ptr<nuraft::buffer> pack(nuraft::ulong index, nuraft::int32 cnt) override {
    std::vector<nuraft::ptr<nuraft::buffer>> logs;

    const auto count = static_cast<std::size_t>(cnt);

    size_t size_total = 0;
    for (nuraft::ulong ii = index; ii < index + count; ++ii) {
      nuraft::ptr<nuraft::log_entry> le;
      {
        std::lock_guard lg(logs_lock_);
        le = logs_[ii];
      }
      assert(le.get());
      nuraft::ptr<nuraft::buffer> buf = le->serialize();
      size_total += buf->size();
      logs.push_back(buf);
    }

    auto buf_out = nuraft::buffer::alloc(sizeof(nuraft::int32) + count * sizeof(nuraft::int32) + size_total);
    buf_out->pos(0);
    buf_out->put((nuraft::int32)cnt);

    for (auto& entry : logs) {
      nuraft::ptr<nuraft::buffer>& bb = entry;
      buf_out->put((nuraft::int32)bb->size());
      buf_out->put(*bb);
    }
    return buf_out;
  }

  void apply_pack(nuraft::ulong index, nuraft::buffer& pack) override {
    pack.pos(0);
    const auto num_logs = static_cast<std::size_t>(pack.get_int());

    for (std::size_t ii = 0; ii < num_logs; ++ii) {
      const auto cur_idx = index + ii;
      const auto buf_size = static_cast<std::size_t>(pack.get_int());

      nuraft::ptr<nuraft::buffer> buf_local = nuraft::buffer::alloc(buf_size);
      pack.get(buf_local);

      nuraft::ptr<nuraft::log_entry> le = nuraft::log_entry::deserialize(*buf_local);
      {
        std::lock_guard<std::mutex> l(logs_lock_);
        logs_[cur_idx] = le;
      }
    }

    {
      std::lock_guard<std::mutex> l(logs_lock_);
      auto entry = logs_.upper_bound(0);
      if (entry != logs_.end()) {
        start_idx_ = entry->first;
      } else {
        start_idx_ = 1;
      }
    }
  }

  bool compact(nuraft::ulong last_log_index) override {
    std::lock_guard<std::mutex> l(logs_lock_);
    for (nuraft::ulong ii = start_idx_; ii <= last_log_index; ++ii) {
      auto entry = logs_.find(ii);
      if (entry != logs_.end()) {
        logs_.erase(entry);
      }
    }

    // WARNING:
    //   Even though nothing has been erased,
    //   we should set `start_idx_` to new index.
    start_idx_ = last_log_index + 1;
    return true;
  }

  bool flush() override { return true; }

  void close() {}

 private:
  static nuraft::ptr<nuraft::log_entry> make_clone(const nuraft::ptr<nuraft::log_entry>& entry) {
    nuraft::ptr<nuraft::log_entry> clone = nuraft::cs_new<nuraft::log_entry>(
        entry->get_term(), nuraft::buffer::clone(entry->get_buf()), entry->get_val_type());
    return clone;
  }

  std::map<nuraft::ulong, nuraft::ptr<nuraft::log_entry>> logs_;
  mutable std::mutex logs_lock_;
  std::atomic<nuraft::ulong> start_idx_;
};

class InMemoryStateManager : public nuraft::state_mgr {
 public:
  InMemoryStateManager(int srv_id, const std::string& endpoint)
      : my_id_(srv_id), cur_log_store_(nuraft::cs_new<InMemoryLogStore>()) {
    my_srv_config_ = nuraft::cs_new<nuraft::srv_config>(srv_id, endpoint);

    // Initial cluster config: contains only one server (myself).
    saved_config_ = nuraft::cs_new<nuraft::cluster_config>();
    saved_config_->get_servers().push_back(my_srv_config_);
  }

  ~InMemoryStateManager() override = default;

  nuraft::ptr<nuraft::cluster_config> load_config() override {
    // Just return in-memory data in this example.
    // May require reading from disk here, if it has been written to disk.
    return saved_config_;
  }

  void save_config(const nuraft::cluster_config& config) override {
    // Just keep in memory in this example.
    // Need to write to disk here, if want to make it durable.
    nuraft::ptr<nuraft::buffer> buf = config.serialize();
    saved_config_ = nuraft::cluster_config::deserialize(*buf);
  }

  void save_state(const nuraft::srv_state& state) override {
    // Just keep in memory in this example.
    // Need to write to disk here, if want to make it durable.
    nuraft::ptr<nuraft::buffer> buf = state.serialize();
    saved_state_ = nuraft::srv_state::deserialize(*buf);
  }

  nuraft::ptr<nuraft::srv_state> read_state() override {
    // Just return in-memory data in this example.
    // May require reading from disk here, if it has been written to disk.
    return saved_state_;
  }

  nuraft::ptr<nuraft::log_store> load_log_store() override { return cur_log_store_; }

  nuraft::int32 server_id() override { return my_id_; }

  void system_exit(const int) override {}

 private:
  int my_id_;
  nuraft::ptr<InMemoryLogStore> cur_log_store_;
  nuraft::ptr<nuraft::srv_config> my_srv_config_;
  nuraft::ptr<nuraft::cluster_config> saved_config_;
  nuraft::ptr<nuraft::srv_state> saved_state_;
};

class StateMachine : public nuraft::state_machine {
 private:
  struct Context {
    nuraft::ptr<nuraft::snapshot> snapshot;
    nuraft::ptr<nuraft::buffer> value;

    Context(nuraft::ptr<nuraft::snapshot> snapshot, nuraft::ptr<nuraft::buffer> value)
        : snapshot(std::move(snapshot)), value(std::move(value)) {}
    Context() = default;
  };

 private:
  std::atomic<uint64_t> last_committed_idx_;
  std::shared_ptr<StorageEngineBase> internal_engine_;

  std::mutex snapshots_mutex_;
  std::map<uint64_t, nuraft::ptr<Context>> snapshots_;

 public:
  explicit StateMachine(std::shared_ptr<StorageEngineBase> engine)
      : last_committed_idx_(0), internal_engine_(std::move(engine)) {}

 private:
  void commit(nuraft::buffer_serializer& bs) {
    const auto get_bytes = [&bs]() -> std::vector<std::byte> {
      std::size_t size = 0;
      const auto bytes = static_cast<const std::byte*>(bs.get_bytes(size));
      return std::vector<std::byte>(bytes, bytes + size);
    };
    const auto instruction = bs.get_u16();
    switch (instruction) {
      case 2: {  // SET
        BOOST_LOG_TRIVIAL(trace) << "do commit: SET";
        const auto kb = get_bytes();
        const auto digest = bs.get_u64();
        const auto vb = get_bytes();
        internal_engine_->set(Key(kb, digest), Value(vb));
        break;
      }
      case 3:  // DEL
        BOOST_LOG_TRIVIAL(trace) << "do commit: DEL";
        internal_engine_->del(Key(get_bytes(), bs.get_u64()));
        break;

      case 0:  // not assigned
      case 1:  // GET
      case 4:  // EXISTS
      case 5:  // KEYS
      default:
        BOOST_LOG_TRIVIAL(error) << "unknown commit instruction: other: " << instruction;
        break;
    }
  }

  nuraft::ptr<nuraft::buffer> dumpStorage() const {
    const auto dumped_res = internal_engine_->dump();
    if (dumped_res.is_err()) {
      return nullptr;
    }
    std::vector<nuraft::ptr<nuraft::buffer>> buffers;

    std::size_t whole_size = 0;
    const auto dumped = dumped_res.unwrap();
    for (const auto& [k, v] : dumped) {
      const auto size = 2 + 4 + k.bytes().size() + 8 + 4 + v.bytes().size();
      auto buffer = nuraft::buffer::alloc(size);
      whole_size += size;

      nuraft::buffer_serializer bs(buffer);
      bs.put_u16(2);  // SET
      EncodeMessage(bs, k);
      bs.put_u64(k.digest());
      EncodeMessage(bs, v);
      buffers.emplace_back(buffer);
    }

    auto buffer = nuraft::buffer::alloc(whole_size);
    for (const auto& b : buffers) {
      buffer->put(*b);
    }
    return buffer;
  }

 private:
  void createSnapshot(nuraft::snapshot& s) {
    const auto snapshot_buffer = s.serialize();
    const auto ss = nuraft::snapshot::deserialize(*snapshot_buffer);

    const auto dumped = dumpStorage();
    if (!dumped) {
      return;
    }
    const auto context = nuraft::cs_new<Context>(ss, dumped);
    snapshots_[s.get_last_log_idx()] = context;

    auto entry = std::begin(snapshots_);
    for (std::size_t i = 0, count = snapshots_.size(); i < (count - 3); ++i) {
      if (entry == std::end(snapshots_)) {
        break;
      }
      entry = snapshots_.erase(entry);
    }
  }

 public:
  nuraft::ptr<nuraft::buffer> pre_commit(const nuraft::ulong, nuraft::buffer&) override { return nullptr; }

  nuraft::ptr<nuraft::buffer> commit(const nuraft::ulong log_idx, nuraft::buffer& data) override {
    BOOST_LOG_TRIVIAL(trace) << "commit";
    nuraft::buffer_serializer bs(data);
    commit(bs);
    last_committed_idx_ = log_idx;

    nuraft::ptr<nuraft::buffer> ret = nuraft::buffer::alloc(sizeof(log_idx));
    nuraft::buffer_serializer rbs(ret);
    rbs.put_u64(log_idx);
    return ret;
  }

  bool apply_snapshot(nuraft::snapshot& s) override {
    BOOST_LOG_TRIVIAL(trace) << "apply snapshot";
    auto entry = snapshots_.find(s.get_last_log_idx());
    if (entry == std::end(snapshots_)) {
      return false;
    }
    auto buffer = entry->second->value;
    nuraft::buffer_serializer bs(buffer);
    while (bs.pos() < bs.size()) {
      commit(bs);
    }
    return true;
  }

  nuraft::ptr<nuraft::snapshot> last_snapshot() override {
    BOOST_LOG_TRIVIAL(trace) << "last snapshot";
    std::lock_guard<std::mutex> lg(snapshots_mutex_);
    auto entry = std::rbegin(snapshots_);
    if (entry == std::rend(snapshots_)) return nullptr;

    return entry->second->snapshot;
  }

  nuraft::ulong last_commit_index() override { return last_committed_idx_; }

  void create_snapshot(nuraft::snapshot& s, nuraft::async_result<bool>::handler_type& when_done) override {
    BOOST_LOG_TRIVIAL(trace) << "create snapshot";
    {
      std::lock_guard lg(snapshots_mutex_);
      createSnapshot(s);
    }

    bool res = true;
    nuraft::ptr<std::exception> e = nullptr;
    when_done(res, e);
  }
};

}  // namespace detail

class RaftStorageEngine : public StorageEngineBase {
 private:
  nuraft::ptr<nuraft::state_machine> state_machine_;
  nuraft::ptr<nuraft::state_mgr> state_manager_;
  nuraft::raft_launcher launcher_;
  nuraft::ptr<nuraft::raft_server> raft_server_;
  std::shared_ptr<StorageEngineBase> internal_engine_;

 public:
  explicit RaftStorageEngine(const std::shared_ptr<StorageEngineBase>& engine, std::uint16_t raft_port)
      : state_machine_(nuraft::cs_new<detail::StateMachine>(engine)),
        state_manager_(nuraft::cs_new<detail::InMemoryStateManager>(std::random_device{}(),
                                                                    "0.0.0.0:" + std::to_string(raft_port))),
        internal_engine_(engine) {
    raft_server_ = launcher_.init(state_machine_, state_manager_, nuraft::cs_new<detail::Logger>(), raft_port,
                                  nuraft::asio_service::options{}, nuraft::raft_params{});
    while (!raft_server_->is_initialized()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  ~RaftStorageEngine() override { raft_server_->shutdown(); }

 public:
  Result<void> set(const Key& key, const Value& value) override {
    auto buf = nuraft::buffer::alloc(2 + 4 + key.bytes().size() + 8 + 4 + value.bytes().size());
    nuraft::buffer_serializer bs(buf);
    bs.put_u16(2);
    detail::EncodeMessage(bs, key);
    bs.put_u64(key.digest());
    detail::EncodeMessage(bs, value);

    const auto res = raft_server_->append_entries({buf});
    return Result<void>::Ok();
  }

  Result<Value> get(const Key& key) override { return internal_engine_->get(key); }

  Result<void> del(const Key& key) override {
    auto buf = nuraft::buffer::alloc(2 + 4 + key.bytes().size() + 8);
    nuraft::buffer_serializer bs(buf);
    bs.put_u16(3);
    detail::EncodeMessage(bs, key);
    bs.put_u64(key.digest());

    raft_server_->append_entries({buf});
    return Result<void>::Ok();
  }

  Result<bool> exists(const Key& key) override { return internal_engine_->exists(key); }

  Result<std::vector<Key>> keys(const std::string& pattern) override { return internal_engine_->keys(pattern); }

  Result<std::vector<std::tuple<Key, Value>>> dump() override { return internal_engine_->dump(); }
};

}  // namespace eidos::storage