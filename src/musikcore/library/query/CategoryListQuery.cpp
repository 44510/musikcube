//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include "pch.hpp"
#include "CategoryListQuery.h"
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/sdk/String.h>
#include <musikcore/db/Statement.h>
#include <musikcore/i18n/Locale.h>
#include <musikcore/utfutil.h>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core::sdk;
using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;

static const std::string kUnfilteredPlaylistsQuery =
    "SELECT DISTINCT id, name "
    "FROM playlists "
    "ORDER BY name;";

static const std::string kFilteredPlaylistsQuery =
    "SELECT DISTINCT id, name "
    "FROM playlists"
    "WHERE LOWER(name) {{match_type}} ? "
    "ORDER BY name;";

const std::string CategoryListQuery::kQueryName = "CategoryListQuery";

static std::string getMatchType(CategoryListQuery::MatchType matchType) {
    return matchType == CategoryListQuery::MatchType::Regex ? "REGEXP" : "LIKE";
}

CategoryListQuery::CategoryListQuery(
    MatchType matchType,
    const std::string& trackField,
    const std::string& filter)
: CategoryListQuery(matchType, trackField, category::PredicateList(), filter) {
}

CategoryListQuery::CategoryListQuery(
    MatchType matchType,
    const std::string& trackField,
    const category::Predicate predicate,
    const std::string& filter)
: CategoryListQuery(matchType, trackField, category::PredicateList{ predicate }, filter) {
}

CategoryListQuery::CategoryListQuery(
    MatchType matchType,
    const std::string& trackField,
    const category::PredicateList predicates,
    const std::string& filter)
: matchType(matchType)
, trackField(trackField)
, filter(filter) {
    result = std::make_shared<SdkValueList>();

    if (this->filter.size() && this->matchType == MatchType::Substring) {
        /* transform "FilteR" => "%filter%" */
        std::string wild = this->filter;
        std::transform(wild.begin(), wild.end(), wild.begin(), tolower);
        this->filter = "%" + wild + "%";
    }

    category::SplitPredicates(predicates, this->regular, this->extended);

    auto end = category::REGULAR_PROPERTY_MAP.end();

    if (trackField == "playlists") {
        this->outputType = OutputType::Playlist;
    }
    else if (category::GetPropertyType(trackField) == category::PropertyType::Regular) {
        this->outputType = OutputType::Regular;
    }
    else {
        this->outputType = OutputType::Extended;
    }
}

CategoryListQuery::Result CategoryListQuery::GetResult() noexcept {
    return this->result;
}

musik::core::sdk::IValueList* CategoryListQuery::GetSdkResult() {
    return new SdkValueList(this->result);
}

int CategoryListQuery::GetIndexOf(int64_t id) {
    auto result = this->GetResult();
    for (size_t i = 0; i < result->Count(); i++) {
        if (id == result->GetAt(i)->GetId()) {
            return (int) i;
        }
    }
    return -1;
}

void CategoryListQuery::QueryPlaylist(musik::core::db::Connection& db) {
    const bool filtered = this->filter.size();

    std::string query = filtered
        ? kFilteredPlaylistsQuery
        : kUnfilteredPlaylistsQuery;

    category::ReplaceAll(query, "{{match_type}}", getMatchType(matchType));

    Statement stmt(query.c_str() , db);

    if (filtered) {
        stmt.BindText(0, this->filter);
    }

    ProcessResult(stmt);
}

void CategoryListQuery::QueryRegular(musik::core::db::Connection &db) {
    category::ArgumentList args;

    /* order of operations with args is important! otherwise bind params
    will be out of order! */
    auto prop = category::REGULAR_PROPERTY_MAP[this->trackField];
    std::string query = category::REGULAR_PROPERTY_QUERY;
    std::string extended = InnerJoinExtended(this->extended, args);
    std::string regular = JoinRegular(this->regular, args, " AND ");
    std::string regularFilter;

    if (this->filter.size()) {
        regularFilter = category::REGULAR_FILTER;
        category::ReplaceAll(regularFilter, "{{table}}", prop.first);
        category::ReplaceAll(regularFilter, "{{match_type}}", getMatchType(matchType));
        args.push_back(category::StringArgument(this->filter));
    }

    category::ReplaceAll(query, "{{table}}", prop.first);
    category::ReplaceAll(query, "{{fk_id}}", prop.second);
    category::ReplaceAll(query, "{{extended_predicates}}", extended);
    category::ReplaceAll(query, "{{regular_predicates}}", regular);
    category::ReplaceAll(query, "{{regular_filter}}", regularFilter);

    Statement stmt(query.c_str(), db);
    Apply(stmt, args);
    ProcessResult(stmt);
}

void CategoryListQuery::QueryExtended(musik::core::db::Connection &db) {
    category::ArgumentList args;

    /* order of operations with args is important! otherwise bind params
    will be out of order! */
    std::string query = category::EXTENDED_PROPERTY_QUERY;
    std::string regular = category::JoinRegular(this->regular, args, " AND ");
    std::string extended = category::InnerJoinExtended(this->extended, args);
    std::string extendedFilter;

    if (this->filter.size()) {
        extendedFilter = category::EXTENDED_FILTER;
        args.push_back(category::StringArgument(this->filter));
        category::ReplaceAll(extendedFilter, "{{match_type}}", getMatchType(matchType));
    }

    category::ReplaceAll(query, "{{regular_predicates}}", regular);
    category::ReplaceAll(query, "{{extended_predicates}}", extended);
    category::ReplaceAll(query, "{{extended_filter}}", extendedFilter);

    args.push_back(category::StringArgument(this->trackField));

    Statement stmt(query.c_str(), db);
    Apply(stmt, args);
    ProcessResult(stmt);
}

void CategoryListQuery::ProcessResult(musik::core::db::Statement &stmt) {
    SdkValueList unknowns;
    while (stmt.Step() == Row) {
        int64_t id = stmt.ColumnInt64(0);
        std::string displayValue = str::Trim(stmt.ColumnText(1));

        /* we track empty / blank values separately, then sort them to the bottom
        of the returned list so they don't pollute the first results */
        if (!displayValue.size()) {
            unknowns.Add(std::make_shared<SdkValue>(
                u8fmt(_TSTR("unknown_category_value"), unknowns.Count() + 1),
                id,
                this->trackField
            ));
        }
        else {
            result->Add(std::make_shared<SdkValue>(displayValue, id, this->trackField));
        }
    }

    for (size_t i = 0; i < unknowns.Count(); i++) {
        result->Add(unknowns.At(i));
    }
}

bool CategoryListQuery::OnRun(Connection& db) {
    result = std::make_shared<SdkValueList>();

    switch (this->outputType) {
        case OutputType::Playlist: QueryPlaylist(db); break;
        case OutputType::Regular: QueryRegular(db); break;
        case OutputType::Extended: QueryExtended(db); break;
    }

    return true;
}

/* ISerializableQuery */

std::string CategoryListQuery::SerializeQuery() {
    nlohmann::json query;
    query["name"] = kQueryName;
    query["options"] = {
        { "trackField", this->trackField },
        { "filter", this->filter },
        { "matchType", this->matchType },
        { "outputType", this->outputType },
        { "regularPredicateList", PredicateListToJson(this->regular) },
        { "extendedPredicateList", PredicateListToJson(this->extended) }
    };
    return query.dump();
}

std::string CategoryListQuery::SerializeResult() {
    nlohmann::json result = {
        { "result", ValueListToJson(*this->result) }
    };
    return result.dump();
}

void CategoryListQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    auto json = nlohmann::json::parse(data);
    this->result = std::make_shared<SdkValueList>();
    ValueListFromJson(json["result"], *this->result);
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<CategoryListQuery> CategoryListQuery::DeserializeQuery(const std::string& data) {
    nlohmann::json options = nlohmann::json::parse(data)["options"];
    std::shared_ptr<CategoryListQuery> result(new CategoryListQuery());
    result->trackField = options.value("trackField", "");
    result->filter = options.value("filter", "");
    result->matchType = options.value("matchType", MatchType::Substring);
    result->outputType = static_cast<OutputType>(options.value("outputType", OutputType::Regular));
    PredicateListFromJson(options["regularPredicateList"], result->regular);
    PredicateListFromJson(options["extendedPredicateList"], result->extended);
    return result;
}
