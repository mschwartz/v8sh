#include "v8.h"
#include "libplatform/libplatform.h"
#include "assert.h"
#include <string.h>

using namespace v8;

// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
}

void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {
    v8::HandleScope handle_scope(isolate);
    v8::String::Utf8Value exception(try_catch->Exception());
    const char* exception_string = ToCString(exception);
    v8::Handle<v8::Message> message = try_catch->Message();
    if (message.IsEmpty()) {
        // V8 didn't provide any extra information about this error; just
        // print the exception.
        fprintf(stderr, "%s\n", exception_string);
    } 
    else {
        // Print (filename):(line number): (message).
        v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
        const char* filename_string = ToCString(filename);
        int linenum = message->GetLineNumber();
        fprintf(stderr, "%s:%i: %s\n", filename_string, linenum, exception_string);
        // Print line of source code.
        v8::String::Utf8Value sourceline(message->GetSourceLine());
        const char* sourceline_string = ToCString(sourceline);
        fprintf(stderr, "%s\n", sourceline_string);
        // Print wavy underline (GetUnderline is deprecated).
        int start = message->GetStartColumn();
        for (int i = 0; i < start; i++) {
            fprintf(stderr, " ");
        }
        int end = message->GetEndColumn();
        for (int i = start; i < end; i++) {
            fprintf(stderr, "^");
        }
        fprintf(stderr, "\n");
        v8::String::Utf8Value stack_trace(try_catch->StackTrace());
        if (stack_trace.length() > 0) {
            const char* stack_trace_string = ToCString(stack_trace);
            fprintf(stderr, "%s\n", stack_trace_string);
        }
    }
}

// Executes a string within the current v8 context.
bool ExecuteString(v8::Isolate* isolate,
                                     v8::Handle<v8::String> source,
                                     v8::Handle<v8::Value> name,
                                     bool print_result,
                                     bool report_exceptions) {
    v8::HandleScope handle_scope(isolate);
    v8::TryCatch try_catch;
    v8::ScriptOrigin origin(name);
    v8::Handle<v8::Script> script = v8::Script::Compile(source, &origin);
    if (script.IsEmpty()) {
        // Print errors that happened during compilation.
        if (report_exceptions) {
            printf("Exception\n");
            ReportException(isolate, &try_catch);
        }
        return false;
    } 
    else {
        v8::Handle<v8::Value> result = script->Run();
        if (result.IsEmpty()) {
            assert(try_catch.HasCaught());
            // Print errors that happened during execution.
            if (report_exceptions) {
                printf("Exception2\n");
                ReportException(isolate, &try_catch);
            }
            return false;
        } 
        else {
            assert(!try_catch.HasCaught());
            if (print_result && !result->IsUndefined()) {
                // If all went well and the result wasn't undefined then print
                // the returned value.
                v8::String::Utf8Value str(result);
                const char* cstr = ToCString(str);
                printf("%s\n", cstr);
            }
            return true;
        }
    }
}

// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(v8::Isolate* isolate, const char* name) {
    FILE* file = fopen(name, "rb");
    if (file == NULL) return v8::Handle<v8::String>();

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char* chars = new char[size + 1];
    chars[size] = '\0';
    for (size_t i = 0; i < size;) {
        i += fread(&chars[i], 1, size - i, file);
        if (ferror(file)) {
            fclose(file);
            return v8::Handle<v8::String>();
        }
    }
    fclose(file);
    v8::Handle<v8::String> result = v8::String::NewFromUtf8(
            isolate, chars, v8::String::kNormalString, static_cast<int>(size));
    delete[] chars;
    return result;
}

// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
void Load(const v8::FunctionCallbackInfo<v8::Value>& args) {
    for (int i = 0; i < args.Length(); i++) {
        v8::HandleScope handle_scope(args.GetIsolate());
        v8::String::Utf8Value file(args[i]);
        if (*file == NULL) {
            args.GetIsolate()->ThrowException(
                    v8::String::NewFromUtf8(args.GetIsolate(), "Error loading file"));
            return;
        }
        v8::Handle<v8::String> source = ReadFile(args.GetIsolate(), *file);
        if (source.IsEmpty()) {
            args.GetIsolate()->ThrowException(
                     v8::String::NewFromUtf8(args.GetIsolate(), "Error loading file"));
            return;
        }
        if (!ExecuteString(args.GetIsolate(),
                                             source,
                                             v8::String::NewFromUtf8(args.GetIsolate(), *file),
                                             false,
                                             false)) {
            args.GetIsolate()->ThrowException(
                    v8::String::NewFromUtf8(args.GetIsolate(), "Error executing file"));
            return;
        }
    }
}

// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
    bool first = true;
    for (int i = 0; i < args.Length(); i++) {
        v8::HandleScope handle_scope(args.GetIsolate());
        if (first) {
            first = false;
        } 
        else {
            printf(" ");
        }
        v8::String::Utf8Value str(args[i]);
        const char* cstr = ToCString(str);
        printf("%s", cstr);
    }
    printf("\n");
    fflush(stdout);
}

void Read(const v8::FunctionCallbackInfo<v8::Value>&args) {
    if (args.Length() != 1) {
        args.GetIsolate()->ThrowException(
                v8::String::NewFromUtf8(args.GetIsolate(), "Bad parameters"));
        return;
    }
    v8::String::Utf8Value file(args[0]);
    if (*file == NULL) {
        args.GetIsolate()->ThrowException(
                v8::String::NewFromUtf8(args.GetIsolate(), "Error loading file"));
        return;
    }
    v8::Handle<v8::String> source = ReadFile(args.GetIsolate(), *file);
    if (source.IsEmpty()) {
        args.GetIsolate()->ThrowException(
                v8::String::NewFromUtf8(args.GetIsolate(), "Error loading file"));
        return;
    }
    args.GetReturnValue().Set(source);
}

// Creates a new execution environment containing the built-in
// functions.
v8::Handle<v8::Context> CreateContext(v8::Isolate* isolate) {
    // Create a template for the global object.
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    // Bind the global 'print' function to the C++ Print callback.
    global->Set(v8::String::NewFromUtf8(isolate, "print"),
                            v8::FunctionTemplate::New(isolate, Print));
    // Bind the global 'read' function to the C++ Read callback.
    global->Set(v8::String::NewFromUtf8(isolate, "read"),
                            v8::FunctionTemplate::New(isolate, Read));
    // Bind the global 'load' function to the C++ Load callback.
    global->Set(v8::String::NewFromUtf8(isolate, "load"),
                            v8::FunctionTemplate::New(isolate, Load));
    // Bind the 'quit' function
    // global->Set(v8::String::NewFromUtf8(isolate, "quit"),
                            // v8::FunctionTemplate::New(isolate, Quit));
    // Bind the 'version' function
    // global->Set(v8::String::NewFromUtf8(isolate, "version"),
                            // v8::FunctionTemplate::New(isolate, Version));

    return v8::Context::New(isolate, NULL, global);
}


int main(int argc, char* argv[]) {
    // Initialize V8.
    V8::InitializeICU();
    Platform* platform = platform::CreateDefaultPlatform();
    V8::InitializePlatform(platform);
    V8::Initialize();

    // Create a new Isolate and make it the current one.
    Isolate* isolate = Isolate::New();
    {
        Isolate::Scope isolate_scope(isolate);

        // Create a stack-allocated handle scope.
        HandleScope handle_scope(isolate);

        // Create a new context.
        Local<Context> context = CreateContext(isolate);

        // Enter the context for compiling and running the hello world script.
        Context::Scope context_scope(context);

        // Create a string containing the JavaScript source code.
        Local<String> source = ReadFile(isolate, argv[1]); // String::NewFromUtf8(isolate, "'Hello' + ', World!'");

        Local<String> filename = v8::String::NewFromUtf8(isolate, argv[1], v8::String::kNormalString, static_cast<int>(strlen(argv[1])));
        ExecuteString(isolate, source, filename, true, true);
#if 0
        // Compile the source code.
        Local<Script> script = Script::Compile(source);

        // Run the script to get the result.
        Local<Value> result = script->Run();

        // Convert the result to an UTF8 string and print it.
        String::Utf8Value utf8(result);
        printf("%s\n", *utf8);
#endif
    }
    
    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    V8::Dispose();
    V8::ShutdownPlatform();
    delete platform;
    return 0;
}