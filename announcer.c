#include <stdatomic>

atomic_uchar g_running;
atomic_int g_returnCode;

int main(void) {
	return atomic_load(&g_returnCode);
}
