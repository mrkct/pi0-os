#include <kernel/kprintf.h>
#include <kernel/panic.h>
#include <stdint.h> // For uintptr_t etc.
#include <stdio.h> // For snprintf

// Define a common structure for source location, matching UBSan's internal representation
struct SourceLocation {
    const char *file;
    uint32_t line;
    uint32_t column;
};

// Common handler function to print error and panic
// Marked noinline to potentially help debugging stack traces
__attribute__((noinline))
static void ubsan_panic(SourceLocation loc, const char *message) {
    // Ensure interrupts are off before panicking
    irq_disable();
    if (loc.file) {
        kprintf("UBSan: %s at %s:%u:%u\n", message, loc.file, loc.line, loc.column);
    } else {
        kprintf("UBSan: %s at <unknown location>\n", message);
    }
    // Use panic_no_print to avoid potential recursion if kprintf itself has issues
    // or if the panic message formatting causes another UBSan issue.
    panic_no_print("UBSan detected undefined behavior.");
}

// UBSan Handler Functions
extern "C" {

// Type Mismatch (v1)
// Note: TypeDescriptor is complex, we'll just report the location and kind.
struct TypeDescriptor {
  uint16_t type_kind;
  uint16_t type_info;
  char type_name[];
};
struct TypeMismatchDataV1 {
    SourceLocation loc;
    TypeDescriptor *type;
    unsigned char log_alignment;
    unsigned char type_check_kind; // 0: load, 1: store, 2: reference binding, 3: member access, 4: member call, 5: constructor call, ...
};
void __ubsan_handle_type_mismatch_v1(void *data_raw, void *ptr_raw) {
    TypeMismatchDataV1 *data = (TypeMismatchDataV1 *)data_raw;
    uintptr_t ptr = (uintptr_t)ptr_raw;

    const char *type_check_kinds[] = {
        "load of", "store to", "reference binding to", "member access within",
        "member call on", "constructor call on", "downcast of", "downcast of",
        "upcast of", "cast to virtual base of", "_Nonnull binding",
        "dynamic operation on"
    };

    const char *kind_str = (data->type_check_kind < sizeof(type_check_kinds) / sizeof(type_check_kinds[0]))
                           ? type_check_kinds[data->type_check_kind]
                           : "unknown type check";

    char buffer[256]; // Static buffer to avoid allocation
    if (!ptr) {
        snprintf(buffer, sizeof(buffer), "%s null pointer of type %s", kind_str, data->type->type_name);
    } else if (ptr & ((1 << data->log_alignment) - 1)) {
         snprintf(buffer, sizeof(buffer), "%s misaligned address %p for type %s (alignment %u)", kind_str, (void*)ptr, data->type->type_name, 1U << data->log_alignment);
    } else {
         // This case usually means insufficient space, but the message isn't always clear without more type info.
         snprintf(buffer, sizeof(buffer), "%s address %p for type %s (alignment %u)", kind_str, (void*)ptr, data->type->type_name, 1U << data->log_alignment);
    }
    ubsan_panic(data->loc, buffer);
}
// Older compilers might use this signature
void __ubsan_handle_type_mismatch(void* data_raw, void* ptr_raw) __attribute__((alias("__ubsan_handle_type_mismatch_v1")));


// Generic Overflow Data
struct OverflowData { SourceLocation loc; TypeDescriptor *type; };

// Addition Overflow
void __ubsan_handle_add_overflow(void *data_raw, [[maybe_unused]] void *lhs_raw, [[maybe_unused]] void *rhs_raw) {
    OverflowData *data = (OverflowData *)data_raw;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "addition overflow (%s)", data->type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Subtraction Overflow
void __ubsan_handle_sub_overflow(void *data_raw, [[maybe_unused]] void *lhs_raw, [[maybe_unused]] void *rhs_raw) {
    OverflowData *data = (OverflowData *)data_raw;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "subtraction overflow (%s)", data->type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Multiplication Overflow
void __ubsan_handle_mul_overflow(void *data_raw, [[maybe_unused]] void *lhs_raw, [[maybe_unused]] void *rhs_raw) {
    OverflowData *data = (OverflowData *)data_raw;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "multiplication overflow (%s)", data->type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Negation Overflow
void __ubsan_handle_negate_overflow(void *data_raw, [[maybe_unused]] void *old_val_raw) {
    OverflowData *data = (OverflowData *)data_raw;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "negation overflow (%s)", data->type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Division by Zero or Overflow
void __ubsan_handle_divrem_overflow(void *data_raw, [[maybe_unused]] void *lhs_raw, [[maybe_unused]] void *rhs_raw) {
    OverflowData *data = (OverflowData *)data_raw;
    // Note: This handles both division by zero and signed division overflow (e.g., INT_MIN / -1)
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "division by zero or signed division overflow (%s)", data->type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Shift out of Bounds
struct ShiftOutOfBoundsData { SourceLocation loc; TypeDescriptor *left_type; TypeDescriptor *right_type; };
void __ubsan_handle_shift_out_of_bounds(void *data_raw, [[maybe_unused]] void *lhs_raw, [[maybe_unused]] void *rhs_raw) {
    ShiftOutOfBoundsData *data = (ShiftOutOfBoundsData *)data_raw;
    char buffer[128];
    // Could potentially check lhs/rhs values here if needed
    snprintf(buffer, sizeof(buffer), "shift out of bounds (%s << %s)", data->left_type->type_name, data->right_type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Invalid Builtin Function Use (e.g., __builtin_clz(0))
struct InvalidBuiltinData { SourceLocation loc; unsigned char kind; };
void __ubsan_handle_invalid_builtin(void *data_raw) {
    InvalidBuiltinData *data = (InvalidBuiltinData *)data_raw;
    ubsan_panic(data->loc, "invalid builtin function call");
}

// Out of Bounds Array Indexing
struct OutOfBoundsData { SourceLocation loc; TypeDescriptor *array_type; TypeDescriptor *index_type; };
void __ubsan_handle_out_of_bounds(void *data_raw, [[maybe_unused]] void *index_raw) {
    OutOfBoundsData *data = (OutOfBoundsData *)data_raw;
    char buffer[128];
    // Could potentially report the index value if needed
    snprintf(buffer, sizeof(buffer), "array index out of bounds (%s)", data->array_type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Unreachable Code Execution
struct UnreachableData { SourceLocation loc; };
void __ubsan_handle_builtin_unreachable(void *data_raw) {
    UnreachableData *data = (UnreachableData *)data_raw;
    ubsan_panic(data->loc, "unreachable code executed");
}
// Alias for older compilers/different contexts (e.g., missing return in non-void function)
void __ubsan_handle_missing_return(void *data_raw) __attribute__((alias("__ubsan_handle_builtin_unreachable")));


// VLA Bound Not Positive
struct VLABoundData { SourceLocation loc; TypeDescriptor *type; };
void __ubsan_handle_vla_bound_not_positive(void *data_raw, [[maybe_unused]] void *bound_raw) {
    VLABoundData *data = (VLABoundData *)data_raw;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "variable length array bound not positive (%s)", data->type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Non-Null Argument Check
struct NonnullArgData { SourceLocation loc; SourceLocation attr_loc; int arg_index; };
void __ubsan_handle_nonnull_arg(void *data_raw) {
    NonnullArgData *data = (NonnullArgData *)data_raw;
    char buffer[256];
     if (data->attr_loc.file && data->attr_loc.line != 0) { // Check if attr_loc is valid
        // Use %lu for uint32_t (line/column)
        snprintf(buffer, sizeof(buffer), "null pointer passed as argument %d, declared non-null at %s:%lu:%lu",
              data->arg_index + 1, data->attr_loc.file, data->attr_loc.line, data->attr_loc.column);
    } else {
        snprintf(buffer, sizeof(buffer), "null pointer passed as argument %d, which is declared non-null",
              data->arg_index + 1);
    }
    ubsan_panic(data->loc, buffer);
}

// Non-Null Return Check
// Note: The attribute location might not always be available depending on compiler/context.
struct NonnullReturnData { SourceLocation loc; /* Potentially other fields */ };
struct SourceLocationWrapper { SourceLocation loc; }; // Wrapper needed for some ABI versions
void __ubsan_handle_nonnull_return_v1(void *data_raw, void *attr_loc_raw) {
    // data_raw might be NonnullReturnData* or SourceLocation* depending on ABI version
    // attr_loc_raw might be SourceLocation* or NULL
    SourceLocation loc = ((NonnullReturnData*)data_raw)->loc; // Assume newer ABI first
    SourceLocation *attr_loc = (SourceLocation *)attr_loc_raw;

    // Simple check if data_raw looks like a SourceLocation directly (older ABI)
    if (loc.file == nullptr || loc.line == 0) {
         loc = *(SourceLocation*)data_raw;
    }

    char buffer[256];
    if (attr_loc && attr_loc->file && attr_loc->line != 0) {
        // Use %lu for uint32_t (line/column)
        snprintf(buffer, sizeof(buffer), "null pointer returned from function declared non-null at %s:%lu:%lu",
              attr_loc->file, attr_loc->line, attr_loc->column);
    } else {
        snprintf(buffer, sizeof(buffer), "null pointer returned from function declared non-null");
    }
    ubsan_panic(loc, buffer);
}
// Alias for older compilers - REMOVED due to incompatible signature with v1
// void __ubsan_handle_nonnull_return(void* data_raw) __attribute__((alias("__ubsan_handle_nonnull_return_v1")));


// Pointer Overflow (when pointer arithmetic exceeds representable range)
struct PointerOverflowData { SourceLocation loc; };
void __ubsan_handle_pointer_overflow(void *data_raw, void *base_raw, void *result_raw) {
    PointerOverflowData *data = (PointerOverflowData *)data_raw;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "pointer overflow (base %p, result %p)", base_raw, result_raw);
    ubsan_panic(data->loc, buffer);
}

// Invalid Value Load (e.g., loading uninitialized enum, bool != 0/1)
struct InvalidValueData { SourceLocation loc; TypeDescriptor *type; };
void __ubsan_handle_load_invalid_value(void *data_raw, [[maybe_unused]] void *val_raw) {
    InvalidValueData *data = (InvalidValueData *)data_raw;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "load of invalid value for type %s", data->type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Implicit Conversion Check (e.g., float->int truncation, sign change)
// Often noisy, consider disabling specific kinds via -fno-sanitize=implicit-conversion,...
struct ImplicitConversionData { SourceLocation loc; TypeDescriptor *from_type; TypeDescriptor *to_type; unsigned char kind; };
// Kinds: 0=IntegerTruncation, 1=IntegerSignChange, 2=IntegerTruncationSignChange, 3=FloatCastOverflow, 4=FloatToIntTruncation
void __ubsan_handle_implicit_conversion(void *data_raw, [[maybe_unused]] void *from_val_raw, [[maybe_unused]] void *to_val_raw) {
    ImplicitConversionData *data = (ImplicitConversionData *)data_raw;
    // Example: Only panic on integer truncation
    if (data->kind == 0 /* integer truncation */ || data->kind == 2 /* truncation + sign change */) {
        char buffer[200];
        snprintf(buffer, sizeof(buffer), "implicit conversion causing truncation (%s to %s)",
                 data->from_type->type_name, data->to_type->type_name);
        ubsan_panic(data->loc, buffer);
    }
    // Default: Ignore other kinds (like sign change) to avoid excessive panics
    (void)from_val_raw;
    (void)to_val_raw;
}

// Invalid Object Size Check (accessing object via pointer to type larger than the object)
struct InvalidObjectSizeData { SourceLocation loc; /* Potentially other fields */ };
void __ubsan_handle_invalid_object_size(void *data_raw, void *ptr_raw) {
    // data_raw might be SourceLocation* or InvalidObjectSizeData*
    SourceLocation loc = *(SourceLocation*)data_raw; // Assume simpler structure first
    if (loc.file == nullptr || loc.line == 0) {
        loc = ((InvalidObjectSizeData*)data_raw)->loc; // Try struct if simple fails
    }
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "access to object %p with invalid size", ptr_raw);
    ubsan_panic(loc, buffer);
}

// Float Cast Overflow - Requires FPU support in kernel if used
struct FloatCastOverflowData { SourceLocation loc; TypeDescriptor *from_type; TypeDescriptor *to_type; };
void __ubsan_handle_float_cast_overflow(void *data_raw, [[maybe_unused]] void *from_val_raw, [[maybe_unused]] void *to_val_raw) {
    FloatCastOverflowData *data = (FloatCastOverflowData *)data_raw;
    char buffer[200];
    snprintf(buffer, sizeof(buffer), "float cast overflow (%s to %s)",
             data->from_type->type_name, data->to_type->type_name);
    ubsan_panic(data->loc, buffer);
}

// Alignment Assumption (e.g., casting char* to int* when address isn't aligned)
struct AlignmentAssumptionData { SourceLocation loc; SourceLocation assumption_loc; TypeDescriptor* type; };
void __ubsan_handle_alignment_assumption(void* data_raw, void* pointer_raw, uintptr_t alignment, uintptr_t offset) {
    AlignmentAssumptionData* data = (AlignmentAssumptionData*)data_raw;
    char buffer[256];
    // Use %u for uintptr_t (alignment, offset)
    if (offset == 0) {
        snprintf(buffer, sizeof(buffer), "alignment assumption of %u bytes failed for pointer %p to type %s",
                 alignment, pointer_raw, data->type->type_name);
    } else {
        snprintf(buffer, sizeof(buffer), "alignment assumption of %u bytes failed for pointer %p (base %p, offset %u) to type %s",
                 alignment, pointer_raw, (char*)pointer_raw - offset, offset, data->type->type_name);
    }
    // Optionally include assumption_loc if available and valid
    // if (data->assumption_loc.file && data->assumption_loc.line != 0) { ... }
    ubsan_panic(data->loc, buffer);
}


} // extern "C"
