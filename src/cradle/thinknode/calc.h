#ifndef CRADLE_THINKNODE_CALC_H
#define CRADLE_THINKNODE_CALC_H

#include <cradle/core.h>
#include <cradle/thinknode/types.hpp>

namespace cradle {

struct http_connection_interface;
struct check_in_interface;

// Post a calculation to Thinknode.
string
post_calculation(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    calculation_request const& request);

// Given a calculation status, get the next status that would represent
// meaningful progress. If the result is none, no further progress is possible.
optional<calculation_status>
get_next_calculation_status(calculation_status current);

// Get the query string repesentation of a calculation status.
string
calc_status_as_query_string(calculation_status status);

// Query the status of a calculation.
calculation_status
query_calculation_status(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calc_id);

// Retrieve a calculation request from Thinknode.
calculation_request
retrieve_calculation_request(
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calc_id);

// Long poll the status of a calculation.
//
// This will continuously long poll the calculation, passing the most recent
// status to :process_status, until no further progress is possible or an
// error occurs.
//
void
long_poll_calculation_status(
    check_in_interface& check_in,
    std::function<void(calculation_status const&)> const& process_status,
    http_connection_interface& connection,
    thinknode_session const& session,
    string const& context_id,
    string const& calc_id);

// Substitute the variables in a Thinknode request for new requests.
calculation_request
substitute_variables(
    std::map<string, calculation_request> const& substitutions,
    calculation_request const& request);

struct calculation_submission_interface
{
    // Submit a calculation to Thinknode and return its ID.
    //
    // If :dry_run is true, then no new calculations will be submitted and the
    // result is only valid if the calculation already exists (hence the
    // optional return type).
    //
    // (The implementation of this can involve one or more levels of caching.)
    //
    virtual optional<string>
    submit(
        thinknode_session const& session,
        string const& context_id,
        calculation_request const& request,
        bool dry_run)
        = 0;
};

// This is an alternative to Thinknode's meta request functionality that uses
// locally generated requests but tries to be as efficient as possible about
// submitting them to Thinknode. It's more responsive than other methods in
// cases where the client is repeatedly submitting many similar requests to
// Thinknode.
//
// In this method, the caller supplies a Thinknode request containing 'let'
// variables that represent repeated subrequests, and rather than submitting
// the entire request, these subrequests are submitted individually and their
// calculation IDs are substituted into higher-level requests in place of the
// 'variable' requests used to reference them. This method has the advantage
// that it can leverage memory and disk caching to avoid resubmitting
// subrequests that have previously been submitted.
//
// The return value is a structure that includes not only the ID of the
// calculation but also information that may be useful for tracking the
// progress of the calculation tree.
//
// If :dry_run is true, then no new calculations will be submitted and the
// result is only valid if the calculation already exists (hence the
// optional return type).
//
optional<let_calculation_submission_info>
submit_let_calculation_request(
    calculation_submission_interface& submitter,
    thinknode_session const& session,
    string const& context_id,
    augmented_calculation_request const& request,
    bool dry_run = false);

struct calculation_retrieval_interface
{
    // Retrieve a calculation request from Thinknode.
    virtual calculation_request
    retrieve(
        thinknode_session const& session,
        string const& context_id,
        string const& calculation_id)
        = 0;
};

// Search within a calculation request and return a list of subcalculation IDs
// that match :search_string.
// Note that currently the search is limited to matching function names.
std::vector<string>
search_calculation(
    calculation_retrieval_interface& retriever,
    thinknode_session const& session,
    string const& context_id,
    string const& calculation_id,
    string const& search_string);

} // namespace cradle

#endif
