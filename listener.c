#include <atspi/atspi.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

atomic_uchar g_running;
atomic_int g_returnCode;

static void maybe_error(GError* inout_pError, unsigned char exit_if_fail) {
	if (!inout_pError) return;

	fprintf(stderr, "Error: [GLib] %s", inout_pError->message ? inout_pError->message : "NULL");
	int code = inout_pError->code;
	g_error_free(inout_pError);
	inout_pError = (void*)0;
	if (exit_if_fail) {
		printf("Exit requested. Shutting down.");
		atomic_store(&g_returnCode, code);
		atomic_store(&g_running, 0);
	}
	else (void) code;
}

static void handle_signal(int signal) {
	switch (signal) {
		case SIGINT:
		case SIGTERM:
			printf("Interrupt signal received. Shutting down.");
			atomic_store(&g_running, 0);
			atomic_store(&g_returnCode, 0);
			break;
		default:
			break;
	}
}

static void initialize_signal_action(struct sigaction* out_pAction) {
	if (!out_pAction) return;

	memset(out_pAction, 0, sizeof(struct sigaction));
	out_pAction->sa_handler = &handle_signal;
	sigemptyset(&out_pAction->sa_mask);
	out_pAction->sa_flags = 0;

	sigaction(SIGINT, out_pAction, (void*)0);
	sigaction(SIGTERM, out_pAction, (void*)0);
}

static const char* atspi_live_to_string(AtspiLive live) {
	switch (live) {
		case ATSPI_LIVE_NONE:
			break;
		case ATSPI_LIVE_POLITE:
			return "Polite";
		case ATSPI_LIVE_ASSERTIVE:
			return "Assertive";
	}

	return "Unknown";
}

typedef enum __detail_type {
	DETAIL_TYPE_UNKNOWN = 0,
	DETAIL_TYPE_STATE,
	DETAIL_TYPE_LIVE,
} detail_type_t;

static const char* detail_type_to_string(enum __detail_type detail) {
	switch (detail) {
		case DETAIL_TYPE_UNKNOWN:
			break;
		case DETAIL_TYPE_STATE:
			return "State";
		case DETAIL_TYPE_LIVE:
			return "Live";
	}

	return "Unknown";
}

static detail_type_t determin_detail_type(const gchar* in_pEventType) {
	if (!in_pEventType) return DETAIL_TYPE_UNKNOWN;

	if (strstr(in_pEventType, "state")) return DETAIL_TYPE_STATE;
	else if (strstr(in_pEventType, "announcement")) return DETAIL_TYPE_LIVE;
	return DETAIL_TYPE_UNKNOWN;
}

static void atspi_event_callback(AtspiEvent* inout_pEvent, void* /*inout_pUserData*/) {
	if (
		!inout_pEvent ||
		!inout_pEvent->type
	) {
		fprintf(stderr, "Error: [AT-SPI] Event received, but it could not be handled because required data is null.");
		return;
	}
	else if (!atomic_load(&g_running)) {
		atspi_event_quit();
		return;
	}

	detail_type_t detail_type = determin_detail_type(inout_pEvent->type);
	const char* detail_data = "NULL";
	const gchar* unpacked_value = "NONE";
	switch (detail_type) {
		case DETAIL_TYPE_UNKNOWN:
		case DETAIL_TYPE_STATE: // Unused yet.
			break;
		case DETAIL_TYPE_LIVE:
			detail_data = atspi_live_to_string(inout_pEvent->detail1);
			unpacked_value = g_value_get_string(&inout_pEvent->any_data);
			break;
	}

	printf(
		"Info: [AT-SPI] Event received\n"
		"  type: %s\n"
		"  Source: %ull\n"
		"  First detail type: %s\n"
		"  First detail data: %s\n",
		"  Unpacked value: %s\n",
		inout_pEvent->type,
		inout_pEvent->source,
		detail_type,
		detail_data,
		unpacked_value ? unpacked_value : "NONE"
	);

	if (G_IS_VALUE(&inout_pEvent->any_data)) g_value_unset(&inout_pEvent->any_data);
	g_boxed_free(ATSPI_TYPE_EVENT, inout_pEvent);
}

int main(void) {
	atomic_store(&g_running, 1);
	atomic_store(&g_returnCode, 0);

	static struct sigaction signal_action;
	initialize_signal_action(&signal_action);

	printf("Starting announcement event listener");

	atomic_store(&g_running, 0);
	return atomic_load(&g_returnCode);
}
