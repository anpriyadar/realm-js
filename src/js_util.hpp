////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>

#include <math.h>
#include <string>
#include <sstream>

#include <stdexcept>
#include <cmath>
#include "property.hpp"

#include "js_compat.hpp"
#include "schema.hpp"

#define WRAP_EXCEPTION(METHOD, EXCEPTION, ARGS...)  \
try { METHOD(ARGS); } \
catch(std::exception &e) { RJSSetException(ctx, EXCEPTION, e); }

#define WRAP_CLASS_METHOD(CLASS_NAME, METHOD_NAME) \
JSValueRef CLASS_NAME ## METHOD_NAME(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* ex) { \
    JSValueRef returnValue = NULL; \
    WRAP_EXCEPTION(CLASS_NAME::METHOD_NAME, *ex, ctx, thisObject, argumentCount, arguments, returnValue); \
    return returnValue; \
}

#define WRAP_CONSTRUCTOR(CLASS_NAME, METHOD_NAME) \
JSObjectRef CLASS_NAME ## METHOD_NAME(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* ex) { \
    JSObjectRef returnObject = NULL; \
    WRAP_EXCEPTION(CLASS_NAME::METHOD_NAME, *ex, ctx, constructor, argumentCount, arguments, returnObject); \
    return returnObject; \
}

#define WRAP_PROPERTY_GETTER(CLASS_NAME, METHOD_NAME) \
JSValueRef CLASS_NAME ## METHOD_NAME(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* ex) { \
    JSValueRef returnValue = NULL; \
    WRAP_EXCEPTION(CLASS_NAME::METHOD_NAME, *ex, ctx, object, returnValue); \
    return returnValue; \
}

#define WRAP_PROPERTY_SETTER(CLASS_NAME, METHOD_NAME) \
bool CLASS_NAME ## METHOD_NAME(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef* ex) { \
    WRAP_EXCEPTION(CLASS_NAME::METHOD_NAME, *ex, ctx, object, value); \
    return true; \
}

// for stol failure (std::invalid_argument) this could be another property that is handled externally, so ignore
#define WRAP_INDEXED_GETTER(CLASS_NAME, METHOD_NAME) \
JSValueRef CLASS_NAME ## METHOD_NAME(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef* ex) { \
    JSValueRef returnValue = NULL; \
    try { \
        size_t index = RJSValidatedPositiveIndex(RJSStringForJSString(property)); \
        CLASS_NAME::METHOD_NAME(ctx, object, index, returnValue); return returnValue; \
    } \
    catch (std::out_of_range &exp) { return JSValueMakeUndefined(ctx); } \
    catch (std::invalid_argument &exp) { return NULL; } \
    catch (std::exception &e) { RJSSetException(ctx, *ex, e); return NULL; } \
}

#define WRAP_INDEXED_SETTER(CLASS_NAME, METHOD_NAME) \
bool CLASS_NAME ## METHOD_NAME(JSContextRef ctx, JSObjectRef object, JSStringRef property, JSValueRef value, JSValueRef* ex) { \
    try { \
        size_t index = RJSValidatedPositiveIndex(RJSStringForJSString(property)); \
        { CLASS_NAME::METHOD_NAME(ctx, object, index, value); return true; } \
    } \
    catch (std::out_of_range &exp) { RJSSetException(ctx, *ex, exp); } \
    catch (std::invalid_argument &exp) { *ex = RJSMakeError(ctx, "Invalid index"); } \
    catch (std::exception &e) { RJSSetException(ctx, *ex, e); } \
    return false; \
}


template<typename T>
inline void RJSFinalize(JSObjectRef object) {
    delete static_cast<T>(JSObjectGetPrivate(object));
    JSObjectSetPrivate(object, NULL);
}

template<typename T>
inline T RJSGetInternal(JSObjectRef jsObject) {
    return static_cast<T>(JSObjectGetPrivate(jsObject));
}

template<typename T>
JSClassRef RJSCreateWrapperClass(const char * name, JSObjectGetPropertyCallback getter = NULL, JSObjectSetPropertyCallback setter = NULL, const JSStaticFunction *funcs = NULL,
                                 JSObjectGetPropertyNamesCallback propertyNames = NULL, JSClassRef parentClass = NULL, const JSStaticValue *values = NULL) {
    JSClassDefinition classDefinition = kJSClassDefinitionEmpty;
    classDefinition.className = name;
    classDefinition.finalize = RJSFinalize<T>;
    classDefinition.getProperty = getter;
    classDefinition.setProperty = setter;
    classDefinition.staticFunctions = funcs;
    classDefinition.getPropertyNames = propertyNames;
    classDefinition.parentClass = parentClass;
    classDefinition.staticValues = values;
    return JSClassCreate(&classDefinition);
}

std::string RJSStringForJSString(JSStringRef jsString);
std::string RJSStringForValue(JSContextRef ctx, JSValueRef value);
std::string RJSValidatedStringForValue(JSContextRef ctx, JSValueRef value, const char * name = nullptr);

JSStringRef RJSStringForString(const std::string &str);
JSValueRef RJSValueForString(JSContextRef ctx, const std::string &str);

inline void RJSValidateArgumentCount(size_t argumentCount, size_t expected, const char *message = NULL) {
    if (argumentCount != expected) {
        throw std::invalid_argument(message ?: "Invalid arguments");
    }
}

inline void RJSValidateArgumentCountIsAtLeast(size_t argumentCount, size_t expected, const char *message = NULL) {
    if (argumentCount < expected) {
        throw std::invalid_argument(message ?: "Invalid arguments");
    }
}

inline void RJSValidateArgumentRange(size_t argumentCount, size_t min, size_t max, const char *message = NULL) {
    if (argumentCount < min || argumentCount > max) {
        throw std::invalid_argument(message ?: "Invalid arguments");
    }
}

class RJSException : public std::runtime_error {
public:
    RJSException(JSContextRef ctx, JSValueRef &ex) : std::runtime_error(RJSStringForValue(ctx, ex)),
        m_jsException(ex) {}
    JSValueRef exception() { return m_jsException; }

private:
    JSValueRef m_jsException;
};

JSValueRef RJSMakeError(JSContextRef ctx, RJSException &exp);
JSValueRef RJSMakeError(JSContextRef ctx, std::exception &exp);
JSValueRef RJSMakeError(JSContextRef ctx, const std::string &message);

bool RJSIsValueArray(JSContextRef ctx, JSValueRef value);
bool RJSIsValueArrayBuffer(JSContextRef ctx, JSValueRef value);
bool RJSIsValueDate(JSContextRef ctx, JSValueRef value);

static inline JSObjectRef RJSValidatedValueToObject(JSContextRef ctx, JSValueRef value, const char *message = NULL) {
    JSObjectRef object = JSValueToObject(ctx, value, NULL);
    if (!object) {
        throw std::runtime_error(message ?: "Value is not an object.");
    }
    return object;
}

static inline JSObjectRef RJSValidatedValueToDate(JSContextRef ctx, JSValueRef value, const char *message = NULL) {
    JSObjectRef object = JSValueToObject(ctx, value, NULL);
    if (!object || !RJSIsValueDate(ctx, object)) {
        throw std::runtime_error(message ?: "Value is not a date.");
    }
    return object;
}

static inline JSObjectRef RJSValidatedValueToFunction(JSContextRef ctx, JSValueRef value, const char *message = NULL) {
    JSObjectRef object = JSValueToObject(ctx, value, NULL);
    if (!object || !JSObjectIsFunction(ctx, object)) {
        throw std::runtime_error(message ?: "Value is not a function.");
    }
    return object;
}

static inline double RJSValidatedValueToNumber(JSContextRef ctx, JSValueRef value) {
    if (JSValueIsNull(ctx, value)) {
        throw std::invalid_argument("`null` is not a number.");
    }
    
    JSValueRef exception = NULL;
    double number = JSValueToNumber(ctx, value, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    if (isnan(number)) {
        throw std::invalid_argument("Value not convertible to a number.");
    }
    return number;
}

static inline double RJSValidatedValueToBoolean(JSContextRef ctx, JSValueRef value) {
    if (!JSValueIsBoolean(ctx, value)) {
        throw std::invalid_argument("Value is not a boolean.");
    }
    return JSValueToBoolean(ctx, value);
}

static inline JSValueRef RJSValidatedPropertyValue(JSContextRef ctx, JSObjectRef object, JSStringRef property) {
    JSValueRef exception = NULL;
    JSValueRef propertyValue = JSObjectGetProperty(ctx, object, property, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    return propertyValue;
}

static inline JSValueRef RJSValidatedPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned int index) {
    JSValueRef exception = NULL;
    JSValueRef propertyValue = JSObjectGetPropertyAtIndex(ctx, object, index, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    return propertyValue;
}

static inline JSObjectRef RJSValidatedObjectProperty(JSContextRef ctx, JSObjectRef object, JSStringRef property, const char *err = NULL) {
    JSValueRef propertyValue = RJSValidatedPropertyValue(ctx, object, property);
    if (JSValueIsUndefined(ctx, propertyValue)) {
        throw std::runtime_error(err ?: "Object property '" + RJSStringForJSString(property) + "' is undefined");
    }
    return RJSValidatedValueToObject(ctx, propertyValue, err);
}

static inline JSObjectRef RJSValidatedObjectAtIndex(JSContextRef ctx, JSObjectRef object, unsigned int index) {
    return RJSValidatedValueToObject(ctx, RJSValidatedPropertyAtIndex(ctx, object, index));
}

static inline std::string RJSValidatedStringProperty(JSContextRef ctx, JSObjectRef object, JSStringRef property) {
    JSValueRef exception = NULL;
    JSValueRef propertyValue = JSObjectGetProperty(ctx, object, property, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    return RJSValidatedStringForValue(ctx, propertyValue, RJSStringForJSString(property).c_str());
}

static inline size_t RJSValidatedListLength(JSContextRef ctx, JSObjectRef object) {
    JSValueRef exception = NULL;
    static JSStringRef lengthString = JSStringCreateWithUTF8CString("length");
    JSValueRef lengthValue = JSObjectGetProperty(ctx, object, lengthString, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
    if (!JSValueIsNumber(ctx, lengthValue)) {
        throw std::runtime_error("Missing property 'length'");
    }

    return RJSValidatedValueToNumber(ctx, lengthValue);
}

static inline void RJSValidatedSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes = 0) {
    JSValueRef exception = NULL;
    JSObjectSetProperty(ctx, object, propertyName, value, attributes, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
}

template<typename T>
T stot(const std::string s) {
    std::istringstream iss(s);
    T value;
    iss >> value;
    if (iss.fail()) {
        throw std::invalid_argument("Cannot convert string '" + s + "'");
    }
    return value;
}

static inline size_t RJSValidatedPositiveIndex(std::string indexStr) {
    long index = stot<long>(indexStr);
    if (index < 0) {
        throw std::out_of_range(std::string("Index ") + indexStr + " cannot be less than zero.");
    }
    return index;
}

static inline bool RJSIsValueObjectOfType(JSContextRef ctx, JSValueRef value, JSStringRef type) {
    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);

    JSValueRef exception = NULL;
    JSValueRef constructorValue = JSObjectGetProperty(ctx, globalObject, type, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }

    bool ret = JSValueIsInstanceOfConstructor(ctx, value, RJSValidatedValueToObject(ctx, constructorValue), &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }

    return ret;
}

static inline void RJSSetReturnUndefined(JSContextRef ctx, JSValueRef &returnObject) {
    returnObject = JSValueMakeUndefined(ctx);
}

template<typename T>
static inline void RJSSetReturnNumber(JSContextRef ctx, JSValueRef &returnObject, T number) {
    returnObject = JSValueMakeNumber(ctx, number);
}

static inline void RJSSetReturnArray(JSContextRef ctx, size_t count, const JSValueRef *objects, JSValueRef &returnObject) {
    returnObject = JSObjectMakeArray(ctx, count, objects, NULL);
}

static inline void RJSSetException(JSContextRef ctx, JSValueRef &exceptionObject, std::exception &exception) {
    exceptionObject = RJSMakeError(ctx, exception);
}

static JSObjectRef RJSDictForPropertyArray(JSContextRef ctx, const realm::ObjectSchema &object_schema, JSObjectRef array) {
    // copy to dictionary
    if (object_schema.properties.size() != RJSValidatedListLength(ctx, array)) {
        throw std::runtime_error("Array must contain values for all object properties");
    }
    
    JSValueRef exception = NULL;
    JSObjectRef dict = JSObjectMake(ctx, NULL, NULL);
    for (unsigned int i = 0; i < object_schema.properties.size(); i++) {
        JSStringRef nameStr = JSStringCreateWithUTF8CString(object_schema.properties[i].name.c_str());
        JSValueRef value = JSObjectGetPropertyAtIndex(ctx, array, i, &exception);
        if (exception) {
            throw RJSException(ctx, exception);
        }
        JSObjectSetProperty(ctx, dict, nameStr, value, kJSPropertyAttributeNone, &exception);
        if (exception) {
            throw RJSException(ctx, exception);
        }
        JSStringRelease(nameStr);
    }
    return dict;
}

static void RJSCallFunction(JSContextRef ctx, JSObjectRef function, JSObjectRef object, size_t argumentCount, const JSValueRef *arguments) {
    JSValueRef exception = NULL;
    JSObjectCallAsFunction(ctx, function, object, argumentCount, arguments, &exception);
    if (exception) {
        throw RJSException(ctx, exception);
    }
}


static bool RJSValueIsObjectOfClass(JSContextRef ctx, JSValueRef value, JSClassRef jsClass) {
    return JSValueIsObjectOfClass(ctx, value, jsClass);
}


