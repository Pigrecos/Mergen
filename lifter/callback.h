// In un header file, ad esempio "callback.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*OutputCallback)(const char* message);

void SetOutputCallback(OutputCallback callback);

#ifdef __cplusplus
}
#endif