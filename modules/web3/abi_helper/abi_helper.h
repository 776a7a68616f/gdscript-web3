#ifndef ABI_HELPER_H
#define ABI_HELPER_H

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/variant.h"
#include "core/error/error_macros.h"
#include "core/error/error_list.h"

#include "core/io/marshalls.h"
#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/io/json.h"
#include "modules/jsonrpc/jsonrpc.h"
#include "scene/main/http_request.h"

#include <gmp.h>

#include "abi_util.h"
#include "abimethod.h"
#include "abievent.h"
#include "abierror.h"
#include "abiargument.h"


class ABIHelper : public RefCounted {
    GDCLASS(ABIHelper, RefCounted)

protected:
	static void _bind_methods();

public:
    ABIHelper();
    ~ABIHelper();

    ABIMethod* m_constructor;
    Vector<ABIMethod*> m_methods;
    Dictionary m_methods_index; // key: method name, value: index for ABIMethod object pointer in m_methods

    Vector<ABIEvent*> m_events;
    Dictionary m_events_index; // key: method name, value: index for ABIEvent object pointer in m_events

    Vector<ABIError*> m_errors;
    Dictionary m_errors_index; // key: method name, value: index for ABIError object pointer in m_errors

    ABIMethod* m_fallback;
    ABIMethod* m_receive;

    bool unmarshal_from_json(const String &json_str);
    PackedByteArray pack(String name, const Array &args);
    Error unpack_into_dictionary(String name, PackedByteArray data, Dictionary result);
    Error unpack_into_array(String name, PackedByteArray data, Array result);

    // method opera functions
    void add_abimethod(const String &name, ABIMethod* method);
    ABIMethod* get_abimethod(const String &name);
    int abimethod_count();

    // event opera functions
    void add_abievent(const String &name, ABIEvent* event);
    ABIEvent* get_abievent(const String &name);
    int abievent_count();

    // error opera functions
    void add_abierror(const String &name, ABIError* error);
    ABIError* get_abierror(const String &name);
    int abierror_count();

    bool has_fallback();
    bool has_receive();

    ABIArguments get_arguments(const String &name, const PackedByteArray &data);

	// debug methods
	void format_output() const;
};

#endif // ABI_HELPER_H
