#ifdef __cplusplus
extern "C" {
#endif


#define NORETURN __attribute__((noreturn))
NORETURN void die(const char *err, ...) __attribute__((format (printf, 1, 2)));


#ifdef __cplusplus
}
#endif
