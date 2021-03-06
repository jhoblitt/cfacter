#include <facter/facts/collection.hpp>
#include <facter/facts/resolver.hpp>
#include <facter/facts/value.hpp>
#include <facter/facts/scalar_value.hpp>
#include <facter/logging/logging.hpp>
#include <facter/util/directory.hpp>
#include <facter/util/dynamic_library.hpp>
#include <facter/util/environment.hpp>
#include <facter/version.h>
#include <boost/filesystem.hpp>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <yaml-cpp/yaml.h>
#include <algorithm>

using namespace std;
using namespace facter::util;
using namespace rapidjson;
using namespace YAML;
using namespace boost::filesystem;

LOG_DECLARE_NAMESPACE("facts.collection");

namespace facter { namespace facts {

    collection::collection()
    {
        // This needs to be defined here since we use incomplete types in the header
    }

    collection::~collection()
    {
        // This needs to be defined here since we use incomplete types in the header
    }

    collection::collection(collection&& other)
    {
        *this = std::move(other);
    }

    collection& collection::operator=(collection&& other)
    {
        if (this != &other) {
            _facts = std::move(other._facts);
            _resolvers = std::move(other._resolvers);
            _resolver_map = std::move(other._resolver_map);
            _pattern_resolvers = std::move(other._pattern_resolvers);
        }
        return *this;
    }

    void collection::add_default_facts()
    {
        add("cfacterversion", make_value<string_value>(LIBFACTER_VERSION));
        add("facterversion", make_value<string_value>(LIBFACTER_VERSION));
        add_platform_facts();
    }

    void collection::add(shared_ptr<resolver> const& res)
    {
        if (!res) {
            return;
        }

        for (auto const& name : res->names()) {
            _resolver_map.insert({ name, res });
        }

        if (res->has_patterns()) {
            _pattern_resolvers.push_back(res);
        }

        _resolvers.push_back(res);
    }

    void collection::add(string&& name, unique_ptr<value>&& value)
    {
        // Ensure the fact is resolved before replacing it
        auto old_value = get_value(name, true);

        if (LOG_IS_DEBUG_ENABLED()) {
            if (old_value) {
                ostringstream old_value_ss;
                old_value->write(old_value_ss);
                if (!value) {
                    LOG_DEBUG("fact \"%1%\" resolved to null and the existing value of %2% will be removed.", name, old_value_ss.str());
                } else {
                    ostringstream new_value_ss;
                    value->write(new_value_ss);
                    LOG_DEBUG("fact \"%1%\" has changed from %2% to %3%.", name, old_value_ss.str(), new_value_ss.str());
                }
            } else {
                if (!value) {
                    LOG_DEBUG("fact \"%1%\" resolved to null and will not be added.", name);
                } else {
                    ostringstream new_value_ss;
                    value->write(new_value_ss);
                    LOG_DEBUG("fact \"%1%\" has resolved to %2%.", name, new_value_ss.str());
                }
            }
        }

        if (!value) {
            if (old_value) {
                remove(name);
            }
            return;
        }

        _facts[move(name)] = move(value);
    }

    void collection::add_external_facts(vector<string> const& directories)
    {
        auto resolvers = get_external_resolvers();

        auto search_directories = directories;
        if (search_directories.empty()) {
            search_directories = get_external_fact_directories();
        }

        // Build a map between a file and the resolver that can resolve it
        map<string, external::resolver const*> files;
        for (auto const& dir : search_directories) {
            boost::system::error_code ec;
            if (!is_directory(dir, ec)) {
                // Warn the user if not using the default search directories
                if (!directories.empty()) {
                    LOG_WARNING("skipping external facts for \"%1%\": %2%", dir, ec.message());
                } else {
                    LOG_DEBUG("skipping external facts for \"%1%\": %2%", dir, ec.message());
                }
                continue;
            }

            LOG_DEBUG("searching \"%1%\" for external facts.", dir);

            directory::each_file(dir, [&](string const& path) {
                for (auto const& res : resolvers) {
                    if (res->can_resolve(path)) {
                        files.emplace(path, res.get());
                        break;
                    }
                }
                return true;
            });
        }

        if (files.empty()) {
            LOG_DEBUG("no external facts were found.");
            return;
        }

        // Resolve the files
        for (auto const& kvp : files) {
            try {
                kvp.second->resolve(kvp.first, *this);
            }
            catch (external::external_fact_exception& ex) {
                LOG_ERROR("error while processing \"%1%\" for external facts: %2%", kvp.first, ex.what());
            }
        }
    }

    void collection::remove(shared_ptr<resolver> const& res)
    {
        if (!res) {
            return;
        }

        // Remove all name associations
        for (auto const& name : res->names()) {
            auto range = _resolver_map.equal_range(name);
            auto it = range.first;
            while (it != range.second) {
                if (it->second != res) {
                    ++it;
                    continue;
                }
                _resolver_map.erase(it++);
            }
        }

        _pattern_resolvers.remove(res);
        _resolvers.remove(res);
    }

    void collection::remove(string const& name)
    {
        // Ensure the fact is in the collection
        // This will properly resolve the fact prior to removing it
        if (!get_value(name, true)) {
            return;
        }

        _facts.erase(name);
    }

    void collection::clear()
    {
        _facts.clear();
        _resolvers.clear();
        _resolver_map.clear();
        _pattern_resolvers.clear();
    }

    bool collection::empty()
    {
        return _facts.empty() && _resolvers.empty();
    }

    size_t collection::size()
    {
        resolve_facts();
        return _facts.size();
    }

    void collection::filter(set<string> const& names, bool add)
    {
        // First resolve all the facts that were named
        for (auto const& name : names) {
            // Ensure the value is resolved; if not, add an empty string fact
            if (!get_value(name, true) && add) {
                LOG_DEBUG("fact \"%1%\" was requested but not resolved; adding empty string value.", name);
                this->add(string(name), make_value<string_value>(""));
            }
        }

        // Next, move them into a new map
        map<string, unique_ptr<value>> filtered_facts;
        for (auto const& name : names) {
            // Move the value into the filtered facts map
            auto it = _facts.find(name);
            if (it == _facts.end()) {
                continue;
            }

            filtered_facts.emplace(make_pair(string(name), move(it->second)));
        }

        clear();

        _facts = move(filtered_facts);
    }

    value const* collection::operator[](string const& name)
    {
        return get_value(name, true);
    }

    void collection::each(function<bool(string const&, value const*)> func)
    {
        resolve_facts();

        find_if(begin(_facts), end(_facts), [&func](map<string, unique_ptr<value>>::value_type const& it) {
            return !func(it.first, it.second.get());
        });
    }

    ostream& collection::write(ostream& stream, format fmt)
    {
        resolve_facts();

        if (fmt == format::hash) {
            write_hash(stream);
        } else if (fmt == format::json) {
            write_json(stream);
        } else if (fmt == format::yaml) {
            write_yaml(stream);
        }
        return stream;
    }

    void collection::resolve_facts()
    {
        // Remove the front of the resolvers list and resolve until no resolvers are left
        while (!_resolvers.empty()) {
            auto resolver = _resolvers.front();
            remove(resolver);
            resolver->resolve(*this);
        }
    }

    void collection::resolve_fact(string const& name)
    {
        // Resolve every resolver mapped to this name first
        auto range = _resolver_map.equal_range(name);
        auto it = range.first;
        while (it != range.second) {
            auto resolver = (it++)->second;
            remove(resolver);
            resolver->resolve(*this);
        }

         // Resolve every resolver that matches the given name
        auto pattern_it = _pattern_resolvers.begin();
        while (pattern_it != _pattern_resolvers.end()) {
            if (!(*pattern_it)->is_match(name)) {
                ++pattern_it;
                continue;
            }
            auto resolver = *(pattern_it++);
            remove(resolver);
            resolver->resolve(*this);
        }
    }

    value const* collection::get_value(string const& name, bool resolve)
    {
        if (resolve) {
            resolve_fact(name);
        }

        // Lookup the fact
        auto it = _facts.find(name);
        return it == _facts.end() ? nullptr : it->second.get();
    }

    void collection::write_hash(ostream& stream) const
    {
        // If there's only one fact, print it without the name
        if (_facts.size() == 1) {
            _facts.begin()->second->write(stream, false);
            return;
        }

        // Print all facts in the map
        bool first = true;
        for (auto const& kvp : _facts) {
            if (first) {
                first = false;
            } else {
                stream << '\n';
            }
            stream << kvp.first << " => ";
            kvp.second->write(stream, false);
        }
    }

    struct stream_adapter
    {
        explicit stream_adapter(ostream& stream) : _stream(stream)
        {
        }

        void Put(char c)
        {
            _stream << c;
        }

     private:
         ostream& _stream;
    };

    void collection::write_json(ostream& stream) const
    {
        Document document;
        document.SetObject();

        for (auto const& kvp : _facts) {
            rapidjson::Value value;
            kvp.second->to_json(document.GetAllocator(), value);
            document.AddMember(kvp.first.c_str(), value, document.GetAllocator());
        }

        stream_adapter adapter(stream);
        PrettyWriter<stream_adapter> writer(adapter);
        writer.SetIndent(' ', 2);
        document.Accept(writer);
    }

    void collection::write_yaml(ostream& stream) const
    {
        Emitter emitter(stream);
        emitter << BeginMap;
        for (auto const& kvp : _facts) {
            emitter << Key << kvp.first << YAML::Value;
            kvp.second->write(emitter);
        }
        emitter << EndMap;
    }

}}  // namespace facter::facts
