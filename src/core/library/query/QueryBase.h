//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Daniel �nnerby
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <core/config.h>
#include <sigslot/sigslot.h>
#include <boost/shared_ptr.hpp>

/* forward decl */
namespace musik { namespace core {
    namespace db {
        class Connection;
    }

    namespace library {
        class  LibraryBase;
        class  LocalLibrary;
    }
} }

namespace musik { namespace core { namespace query {

    class QueryBase;
    typedef boost::shared_ptr<query::QueryBase> Ptr;

    typedef enum {
        AutoCallback = 1,
        Prioritize = 4,
        CancelQueue = 8,
        CancelSimilar = 16,
        UnCanceable = 32,
        CopyUniqueId = 64
    } Options;

    //////////////////////////////////////////
    ///\brief
    ///Interface class for all queries.
    //////////////////////////////////////////

    class QueryBase : public sigslot::has_slots<> {
        public:
            typedef sigslot::signal3<
                query::QueryBase*, 
                library::LibraryBase*, 
                bool> QueryFinishedEvent;

            typedef enum {
                Idle = 1,
                Running = 2,
                Canceled = 3,
                Finished = 4,
            } Status;

            QueryFinishedEvent QueryFinished;

            QueryBase();
            virtual ~QueryBase();

            int GetStatus();
            int GetQueryId();
            int GetOptions();

        protected:
            void SetStatus(int status);
            void SetOptions(int options);

            virtual bool RunCallbacks(library::LibraryBase *library) { return true; };
            virtual bool RunQuery(library::LibraryBase *library, db::Connection &db) = 0;

            virtual std::string Name();

        private:
            unsigned int status;
            unsigned int queryId;
            unsigned int options;
            boost::mutex stateMutex;
    };


//////////////////////////////////////////////////////////////////////////////
} } }   // musik::core::query
//////////////////////////////////////////////////////////////////////////////
