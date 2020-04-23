#include <mongoc/mongoc.h>

#include "mongoc/mongoc-client-private.h"

#include "TestSuite.h"
#include "mock_server/mock-server.h"
#include "mock_server/future-functions.h"

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "client-test-hedged-reads"


static void
test_mongos_hedged_reads_read_pref (void)
{
   mock_server_t *server;
   mongoc_collection_t *collection;
   mongoc_client_t *client;
   bson_t hedge_doc = BSON_INITIALIZER;
   mongoc_read_prefs_t *prefs;
   future_t *future;
   request_t *request;
   bson_error_t error;

   server = mock_mongos_new (5);
   mock_server_run (server);
   client = mongoc_client_new_from_uri (mock_server_get_uri (server));
   collection = mongoc_client_get_collection (client, "db", "collection");

   prefs = mongoc_read_prefs_new (MONGOC_READ_SECONDARY_PREFERRED);

   /* with readPreference mode secondaryPreferred and no hedge, readPreference
    * MUST NOT be sent. */
   mongoc_collection_set_read_prefs (collection, prefs);

   future = future_collection_count (
      collection, MONGOC_QUERY_NONE, NULL, 0, 0, NULL, &error);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{'$readPreference': { '$exists': false }}",
      NULL);

   mock_server_replies_simple (request, "{'ok': 1, 'n': 1}");
   ASSERT_OR_PRINT (1 == future_get_int64_t (future), error);

   request_destroy (request);
   future_destroy (future);

   /* CDRIVER-3583:
    * with readPreference mode secondaryPreferred and hedge set, readPreference
    * MUST be sent. */
   bson_append_bool (&hedge_doc, "enabled", 7, true);
   mongoc_read_prefs_set_hedge (prefs, &hedge_doc);
   mongoc_collection_set_read_prefs (collection, prefs);

   future = future_collection_count (
      collection, MONGOC_QUERY_NONE, NULL, 0, 0, NULL, &error);
   request = mock_server_receives_command (
      server,
      "db",
      MONGOC_QUERY_SLAVE_OK,
      "{'$readPreference': "
      " {'mode': 'secondaryPreferred', 'hedge': {'enabled': true}}}",
      NULL);

   mock_server_replies_simple (request, "{'ok': 1, 'n': 1}");
   ASSERT_OR_PRINT (1 == future_get_int64_t (future), error);

   request_destroy (request);
   future_destroy (future);

   mongoc_read_prefs_destroy (prefs);
   bson_destroy (&hedge_doc);
   mongoc_collection_destroy (collection);
   mongoc_client_destroy (client);
   mock_server_destroy (server);
}


void
test_client_hedged_reads_install (TestSuite *suite)
{
   TestSuite_AddMockServerTest (suite,
                                "/Client/hedged_reads/mongos",
                                test_mongos_hedged_reads_read_pref);
}
