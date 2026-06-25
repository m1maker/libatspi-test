#include <atspi/atspi.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

atomic_uchar g_running;
atomic_int g_returnCode;

static gboolean main_thread_maybe_exit_callback(void* /*inout_pUserData*/) {
	if (!atomic_load(&g_running)) atspi_event_quit();

	return G_SOURCE_REMOVE;
}

static void dispatch_exit_in_main_thread(void) {
	g_main_context_invoke((void*)0, &main_thread_maybe_exit_callback, (void*)0);
}

static void maybe_error(GError* inout_pError, unsigned char exit_if_fail) {
	if (!inout_pError) return;

	fprintf(stderr, "Error: [GLib] %s\n", inout_pError->message ? inout_pError->message : "NULL");
	int code = inout_pError->code;
	g_error_free(inout_pError);
	inout_pError = (void*)0;
	if (exit_if_fail) {
		printf("Exit requested. Shutting down.\n");
		atomic_store(&g_returnCode, code);
		atomic_store(&g_running, 0);
		dispatch_exit_in_main_thread();
	}
	else (void) code;
}

static void handle_signal(int signal) {
	switch (signal) {
		case SIGINT:
		case SIGTERM:
			printf("Interrupt signal received. Shutting down.\n");
			atomic_store(&g_running, 0);
			atomic_store(&g_returnCode, 0);
			dispatch_exit_in_main_thread();
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
		fprintf(stderr, "Error: [AT-SPI] Event received, but it could not be handled because required data is null.\n");
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

	static const char* EVENTS_TO_REGISTER[] = {
		"object:announcement"
	};
	static const unsigned long long int EVENTS_TO_REGISTER_COUNT = sizeof(EVENTS_TO_REGISTER) / sizeof(EVENTS_TO_REGISTER[0]);

	printf("Starting announcement event listener\n");

	int result = atspi_init();
	if (result !=0 && result != 1) {
		fprintf(stderr, "Error: [AT-SPI] Failed to initialize\nCode: %d\n", result);
		atomic_store(&g_returnCode, result);
		atomic_store(&g_running, 0);
		return atomic_load(&g_returnCode);
	}

	AtspiEventListener* pEventListener = atspi_event_listener_new(&atspi_event_callback, /*this*/(void*)0, (void*)0);
	if (
		!pEventListener ||
		!atomic_load(&g_running)
	) return atomic_load(&g_returnCode);

	{
		GError* pError = (void*)0;
		for (
			unsigned long long int i = 0;
			i < EVENTS_TO_REGISTER_COUNT;
			++i
		) {
			atspi_event_listener_register(pEventListener, EVENTS_TO_REGISTER[i], &pError);
			maybe_error(pError, 0);
			pError = (void*)0;
		}
	}

	atspi_event_main();

	for (
		unsigned long long int i = 0;
		i < EVENTS_TO_REGISTER_COUNT;
		++i
	) {
		atspi_event_listener_deregister(pEventListener, EVENTS_TO_REGISTER[i], (void*)0);
	}

	g_object_unref(pEventListener);
	pEventListener = (void*)0;
	atspi_exit();

	atomic_store(&g_running, 0);
	return atomic_load(&g_returnCode);
}
