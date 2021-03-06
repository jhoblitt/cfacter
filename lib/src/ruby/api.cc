#include <facter/ruby/api.hpp>
#include <facter/ruby/ruby_value.hpp>
#include <facter/facts/scalar_value.hpp>
#include <facter/facts/map_value.hpp>
#include <facter/facts/array_value.hpp>
#include <facter/logging/logging.hpp>
#include <facter/util/directory.hpp>
#include <facter/util/environment.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace facter::facts;
using namespace facter::util;
using namespace boost::filesystem;

LOG_DECLARE_NAMESPACE("ruby");

namespace facter { namespace ruby {

#define LOAD_SYMBOL(x) x(reinterpret_cast<decltype(x)>(library.find_symbol(#x, true)))
#define LOAD_ALIASED_SYMBOL(x, y) x(reinterpret_cast<decltype(x)>(library.find_symbol(#x, true, #y)))
#define LOAD_OPTIONAL_SYMBOL(x) x(reinterpret_cast<decltype(x)>(library.find_symbol(#x)))

    api::api(dynamic_library&& library) :
        LOAD_SYMBOL(rb_intern),
        LOAD_SYMBOL(rb_const_get),
        LOAD_SYMBOL(rb_const_set),
        LOAD_SYMBOL(rb_const_remove),
        LOAD_SYMBOL(rb_const_defined),
        LOAD_SYMBOL(rb_define_module),
        LOAD_SYMBOL(rb_define_module_under),
        LOAD_SYMBOL(rb_define_class_under),
        LOAD_SYMBOL(rb_define_method),
        LOAD_SYMBOL(rb_define_singleton_method),
        LOAD_SYMBOL(rb_class_new_instance),
        LOAD_SYMBOL(rb_gv_get),
        LOAD_SYMBOL(rb_funcall),
        LOAD_ALIASED_SYMBOL(rb_funcallv, rb_funcall2),
        LOAD_SYMBOL(rb_proc_new),
        LOAD_SYMBOL(rb_funcall_with_block),
        LOAD_SYMBOL(rb_funcall_passing_block),
        LOAD_SYMBOL(rb_num2ulong),
        LOAD_SYMBOL(rb_num2dbl),
        LOAD_SYMBOL(rb_string_value_ptr),
        LOAD_SYMBOL(rb_rescue2),
        LOAD_SYMBOL(rb_protect),
        LOAD_SYMBOL(rb_jump_tag),
        LOAD_SYMBOL(rb_int2inum),
        LOAD_ALIASED_SYMBOL(rb_str_new_cstr, rb_str_new2),
        LOAD_SYMBOL(rb_load),
        LOAD_SYMBOL(rb_raise),
        LOAD_SYMBOL(rb_block_proc),
        LOAD_SYMBOL(rb_block_given_p),
        LOAD_SYMBOL(rb_gc_register_address),
        LOAD_SYMBOL(rb_gc_unregister_address),
        LOAD_SYMBOL(rb_hash_foreach),
        LOAD_SYMBOL(rb_define_attr),
        LOAD_SYMBOL(rb_ivar_set),
        LOAD_SYMBOL(rb_ivar_get),
        LOAD_ALIASED_SYMBOL(rb_float_new_in_heap, rb_float_new),
        LOAD_ALIASED_SYMBOL(rb_ary_new_capa, rb_ary_new2),
        LOAD_SYMBOL(rb_ary_push),
        LOAD_SYMBOL(rb_ary_entry),
        LOAD_SYMBOL(rb_hash_new),
        LOAD_SYMBOL(rb_hash_aset),
        LOAD_SYMBOL(rb_hash_lookup),
        LOAD_SYMBOL(rb_obj_freeze),
        LOAD_SYMBOL(rb_sym_to_s),
        LOAD_SYMBOL(rb_to_id),
        LOAD_SYMBOL(rb_id2name),
        LOAD_SYMBOL(rb_define_alloc_func),
        LOAD_SYMBOL(rb_data_object_alloc),
        LOAD_SYMBOL(rb_gc_mark),
        LOAD_SYMBOL(rb_yield_values),
        LOAD_SYMBOL(rb_cObject),
        LOAD_SYMBOL(rb_cArray),
        LOAD_SYMBOL(rb_cHash),
        LOAD_SYMBOL(rb_cString),
        LOAD_SYMBOL(rb_cSymbol),
        LOAD_SYMBOL(rb_cFixnum),
        LOAD_SYMBOL(rb_cFloat),
        LOAD_SYMBOL(rb_eException),
        LOAD_SYMBOL(rb_eArgError),
        LOAD_SYMBOL(rb_eTypeError),
        LOAD_SYMBOL(rb_eStandardError),
        LOAD_SYMBOL(rb_eRuntimeError),
        LOAD_OPTIONAL_SYMBOL(ruby_setup),
        LOAD_SYMBOL(ruby_init),
        LOAD_SYMBOL(ruby_options),
        LOAD_SYMBOL(ruby_cleanup),
        _library(move(library)),
        _cleanup(_library.first_load()),
        _initialized(false)
    {
    }

    api::~api()
    {
        if (_initialized && _cleanup) {
            ruby_cleanup(0);
        }
    }

    api* api::instance()
    {
        static unique_ptr<api> instance = create();
        return instance.get();
    }

    unique_ptr<api> api::create()
    {
        dynamic_library library = find_library();
        if (!library.loaded()) {
            LOG_WARNING("could not locate a ruby library: custom facts will not be resolved.");
            return nullptr;
        } else if (library.first_load()) {
            LOG_INFO("ruby loaded from \"%1%\".", library.name());
        } else {
            LOG_INFO("ruby was already loaded.");
        }
        try {
            return unique_ptr<api>(new api(move(library)));
        } catch (missing_import_exception& ex) {
            LOG_WARNING("%1%: custom facts will not be resolved.", ex.what());
            return nullptr;
        }
    }

    void api::initialize()
    {
        if (_initialized) {
            return;
        }

        // Prefer ruby_setup over ruby_init if present (2.0+)
        // If ruby is already initialized, this is a no-op
        if (ruby_setup) {
            ruby_setup();
        } else {
            ruby_init();
        }

        LOG_INFO("using ruby version %1% to resolve custom facts.", to_string(rb_const_get(*rb_cObject, rb_intern("RUBY_VERSION"))));

        if (_library.first_load()) {
            // Run an empty script evaluation
            // ruby_options is a required call as it sets up some important stuff (unfortunately)
            char const* opts[] = {
                "ruby",
                "-e",
                ""
            };

            // Check for bundler; this is the only ruby option we support
            string ruby_opt;
            if (environment::get("RUBYOPT", ruby_opt) && boost::starts_with(ruby_opt, "-rbundler/setup")) {
                environment::set("RUBYOPT", "-rbundler/setup");
            } else {
                // Clear RUBYOPT so that only our options are used.
                environment::set("RUBYOPT", "");
            }

            ruby_options(sizeof(opts) / sizeof(opts[0]), const_cast<char**>(opts));
        }

        // Get the values for nil, true, and false
        // We do this because these are not constant across ruby versions
        _nil = rb_ivar_get(*rb_cObject, rb_intern("@facter_nil"));
        _true = rb_funcall(_nil, rb_intern("nil?"), 0);
        _false = rb_funcall(_true, rb_intern("nil?"), 0);

        _initialized = true;
    }

    bool api::initialized() const
    {
        return _initialized;
    }

    vector<string> api::get_load_path() const
    {
        vector<string> directories;

        array_for_each(rb_gv_get("$LOAD_PATH"), [&](VALUE value) {
            string path = to_string(value);
            // Ignore "." as a load path (present in 1.8.7)
            if (path == ".") {
                return false;
            }
            directories.emplace_back(move(path));
            return true;
        });

        return directories;
    }

    string api::to_string(VALUE v) const
    {
        v = rb_funcall(v, rb_intern("to_s"), 0);
        size_t size = static_cast<size_t>(rb_num2ulong(rb_funcall(v, rb_intern("size"), 0)));
        return string(rb_string_value_ptr(&v), size);
    }

    VALUE api::rescue(function<VALUE()> callback, function<VALUE(VALUE)> rescue) const
    {
        return rb_rescue2(
            RUBY_METHOD_FUNC(callback_thunk),
            reinterpret_cast<VALUE>(&callback),
            RUBY_METHOD_FUNC(rescue_thunk),
            reinterpret_cast<VALUE>(&rescue),
            *rb_eException,
            0);
    }

    VALUE api::protect(int& tag, function<VALUE()> callback) const
    {
        return rb_protect(
            callback_thunk,
            reinterpret_cast<VALUE>(&callback),
            &tag);
    }

    VALUE api::callback_thunk(VALUE parameter)
    {
        auto callback = reinterpret_cast<function<VALUE()>*>(parameter);
        return (*callback)();
    }

    VALUE api::rescue_thunk(VALUE parameter, VALUE exception)
    {
        auto rescue = reinterpret_cast<function<VALUE(VALUE)>*>(parameter);
        return (*rescue)(exception);
    }

    void api::array_for_each(VALUE array, std::function<bool(VALUE)> callback) const
    {
        long size = rb_num2ulong(rb_funcall(array, rb_intern("size"), 0));

        for (long i = 0; i < size; ++i) {
            if (!callback(rb_ary_entry(array, i))) {
                break;
            }
        }
    }

    void api::hash_for_each(VALUE hash, function<bool(VALUE, VALUE)> callback) const
    {
        rb_hash_foreach(hash, reinterpret_cast<int(*)(...)>(hash_for_each_thunk), reinterpret_cast<VALUE>(&callback));
    }

    int api::hash_for_each_thunk(VALUE key, VALUE value, VALUE arg)
    {
        auto callback = reinterpret_cast<function<bool(VALUE, VALUE)>*>(arg);
        return (*callback)(key, value) ? 0 /* continue */ : 1 /* stop */;
    }

    string api::exception_backtrace(VALUE ex) const
    {
        return to_string(
            rb_funcall(
                rb_funcall(ex, rb_intern("backtrace"), 0),
                rb_intern("join"),
                1,
                rb_str_new_cstr("\n")));
    }

    bool api::is_a(VALUE value, VALUE klass) const
    {
        return rb_funcall(value, rb_intern("is_a?"), 1, klass) != 0;
    }

    bool api::is_nil(VALUE value) const
    {
        return value == _nil;
    }

    bool api::is_true(VALUE value) const
    {
        return value == _true;
    }

    bool api::is_false(VALUE value) const
    {
        return value == _false;
    }

    bool api::is_hash(VALUE value) const
    {
        return is_a(value, *rb_cHash);
    }

    bool api::is_array(VALUE value) const
    {
        return is_a(value, *rb_cArray);
    }

    bool api::is_string(VALUE value) const
    {
        return is_a(value, *rb_cString);
    }

    bool api::is_symbol(VALUE value) const
    {
        return is_a(value, *rb_cSymbol);
    }

    bool api::is_fixednum(VALUE value) const
    {
        return is_a(value, *rb_cFixnum);
    }

    bool api::is_float(VALUE value) const
    {
        return is_a(value, *rb_cFloat);
    }

    VALUE api::nil_value() const
    {
        return _nil;
    }

    VALUE api::true_value() const
    {
        return _true;
    }

    VALUE api::false_value() const
    {
        return _false;
    }

    VALUE api::to_ruby(value const* val) const
    {
        if (!val) {
            return _nil;
        }
        if (auto ptr = dynamic_cast<ruby_value const*>(val)) {
            return ptr->value();
        }
        if (auto ptr = dynamic_cast<string_value const*>(val)) {
            return rb_str_new_cstr(ptr->value().c_str());
        }
        if (auto ptr = dynamic_cast<integer_value const*>(val)) {
            return rb_int2inum(static_cast<SIGNED_VALUE>(ptr->value()));
        }
        if (auto ptr = dynamic_cast<boolean_value const*>(val)) {
            return ptr->value() ? _true : _false;
        }
        if (auto ptr = dynamic_cast<double_value const*>(val)) {
            return rb_float_new_in_heap(ptr->value());
        }
        if (auto ptr = dynamic_cast<array_value const*>(val)) {
            volatile VALUE array = rb_ary_new_capa(static_cast<long>(ptr->size()));
            ptr->each([&](value const* element) {
                rb_ary_push(array, to_ruby(element));
                return true;
            });
            return array;
        }
        if (auto ptr = dynamic_cast<map_value const*>(val)) {
            volatile VALUE hash = rb_hash_new();
            ptr->each([&](string const& name, value const* element) {
                rb_hash_aset(hash, rb_str_new_cstr(name.c_str()), to_ruby(element));
                return true;
            });
            return hash;
        }
        return _nil;
    }

    VALUE api::lookup(std::initializer_list<std::string> const& names) const
    {
        volatile VALUE current = *rb_cObject;

        for (auto const& name : names) {
            current = rb_const_get(current, rb_intern(name.c_str()));
        }
        return current;
    }

    bool api::equals(VALUE first, VALUE second) const
    {
        return is_true(rb_funcall(first, rb_intern("eql?"), 1, second));
    }

}}  // namespace facter::ruby
