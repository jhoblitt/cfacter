/**
 * @file
 * Declares the Linux Desktop Management Information (DMI) fact resolver.
 */
#pragma once

#include "../posix/dmi_resolver.hpp"
#include <string>

namespace facter { namespace facts { namespace linux {

    /**
     * Responsible for resolving DMI facts.
     */
    struct dmi_resolver : posix::dmi_resolver
    {
     protected:
        /**
         * Called to resolve all facts the resolver is responsible for.
         * @param facts The fact collection that is resolving facts.
         */
        virtual void resolve_facts(collection& facts);

     private:
        static std::string get_chassis_description(std::string const& type);
    };

}}}  // namespace facter::facts::linux
