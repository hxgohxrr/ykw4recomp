#ifdef _WIN32
#define BINDING_EXPORT __declspec(dllexport)
#else
#define BINDING_EXPORT __attribute__((visibility("default")))
#endif
BINDING_EXPORT int binding_fixture_value(void);
BINDING_EXPORT int binding_fixture_value(void) { return 427; }
