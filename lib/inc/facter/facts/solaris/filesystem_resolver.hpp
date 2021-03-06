/**
 * @file
 * Declares the Solaris file system fact resolver.
 */
#pragma once

#include "../posix/filesystem_resolver.hpp"

namespace facter { namespace facts { namespace solaris {

    /**
     * Responsible for resolving Solaris file system facts.
     */
    struct filesystem_resolver : posix::filesystem_resolver
    {
     protected:
        /**
         * Called to resolve the mountpoints fact.
         * @param facts The fact collection that is resolving facts.
         */
        virtual void resolve_mountpoints(collection& facts);
        /**
         * Called to resolve the filesystems fact.
         * @param facts The fact collection that is resolving facts.
         */
        virtual void resolve_filesystems(collection& facts);
        /**
         * Called to resolve the partitions fact.
         * @param facts The fact collection that is resolving facts.
         */
        virtual void resolve_partitions(collection& facts);
    };

}}}  // namespace facter::facts::solaris
