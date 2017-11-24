#ifndef QEMU_QDEV_PROPERTIES_H
#define QEMU_QDEV_PROPERTIES_H

#include "hw/qdev-core.h"

/*** qdev-properties.c ***/

extern const PropertyInfo qdev_prop_bit;
extern const PropertyInfo qdev_prop_bit64;
extern const PropertyInfo qdev_prop_bool;
extern const PropertyInfo qdev_prop_uint8;
extern const PropertyInfo qdev_prop_uint16;
extern const PropertyInfo qdev_prop_uint32;
extern const PropertyInfo qdev_prop_int32;
extern const PropertyInfo qdev_prop_uint64;
extern const PropertyInfo qdev_prop_int64;
extern const PropertyInfo qdev_prop_size;
extern const PropertyInfo qdev_prop_string;
extern const PropertyInfo qdev_prop_chr;
extern const PropertyInfo qdev_prop_ptr;
extern const PropertyInfo qdev_prop_macaddr;
extern const PropertyInfo qdev_prop_on_off_auto;
extern const PropertyInfo qdev_prop_losttickpolicy;
extern const PropertyInfo qdev_prop_blockdev_on_error;
extern const PropertyInfo qdev_prop_bios_chs_trans;
extern const PropertyInfo qdev_prop_fdc_drive_type;
extern const PropertyInfo qdev_prop_drive;
extern const PropertyInfo qdev_prop_netdev;
extern const PropertyInfo qdev_prop_vlan;
extern const PropertyInfo qdev_prop_pci_devfn;
extern const PropertyInfo qdev_prop_blocksize;
extern const PropertyInfo qdev_prop_pci_host_devaddr;
extern const PropertyInfo qdev_prop_uuid;
extern const PropertyInfo qdev_prop_arraylen;
extern const PropertyInfo qdev_prop_link;

#define DEFINE_PROP(_name, _state, _field, _prop, _type) { \
        .name      = (_name),                                    \
        .info      = &(_prop),                                   \
        .offset    = offsetof(_state, _field)                    \
            + type_check(_type, typeof_field(_state, _field)),   \
        }

#define DEFINE_PROP_SIGNED(_name, _state, _field, _defval, _prop, _type) { \
        .name      = (_name),                                           \
        .info      = &(_prop),                                          \
        .offset    = offsetof(_state, _field)                           \
            + type_check(_type,typeof_field(_state, _field)),           \
        .set_default = true,                                            \
        .defval.i  = (_type)_defval,                                    \
        }

#define DEFINE_PROP_SIGNED_NODEFAULT(_name, _state, _field, _prop, _type) { \
        .name      = (_name),                                           \
        .info      = &(_prop),                                          \
        .offset    = offsetof(_state, _field)                           \
            + type_check(_type, typeof_field(_state, _field)),          \
        }

#define DEFINE_PROP_BIT(_name, _state, _field, _bit, _defval) {  \
        .name      = (_name),                                    \
        .info      = &(qdev_prop_bit),                           \
        .bitnr    = (_bit),                                      \
        .offset    = offsetof(_state, _field)                    \
            + type_check(uint32_t,typeof_field(_state, _field)), \
        .set_default = true,                                     \
        .defval.u  = (bool)_defval,                              \
        }

#define DEFINE_PROP_UNSIGNED(_name, _state, _field, _defval, _prop, _type) { \
        .name      = (_name),                                           \
        .info      = &(_prop),                                          \
        .offset    = offsetof(_state, _field)                           \
            + type_check(_type, typeof_field(_state, _field)),          \
        .set_default = true,                                            \
        .defval.u  = (_type)_defval,                                    \
        }

#define DEFINE_PROP_UNSIGNED_NODEFAULT(_name, _state, _field, _prop, _type) { \
        .name      = (_name),                                           \
        .info      = &(_prop),                                          \
        .offset    = offsetof(_state, _field)                           \
            + type_check(_type, typeof_field(_state, _field)),          \
        }

#define DEFINE_PROP_BIT64(_name, _state, _field, _bit, _defval) {       \
        .name      = (_name),                                           \
        .info      = &(qdev_prop_bit64),                                \
        .bitnr    = (_bit),                                             \
        .offset    = offsetof(_state, _field)                           \
            + type_check(uint64_t, typeof_field(_state, _field)),       \
        .set_default = true,                                            \
        .defval.u  = (bool)_defval,                                     \
        }

#define DEFINE_PROP_BOOL(_name, _state, _field, _defval) {       \
        .name      = (_name),                                    \
        .info      = &(qdev_prop_bool),                          \
        .offset    = offsetof(_state, _field)                    \
            + type_check(bool, typeof_field(_state, _field)),    \
        .set_default = true,                                     \
        .defval.u    = (bool)_defval,                            \
        }

#define PROP_ARRAY_LEN_PREFIX "len-"

/**
 * DEFINE_PROP_ARRAY:
 * @_name: name of the array
 * @_state: name of the device state structure type
 * @_field: uint32_t field in @_state to hold the array length
 * @_arrayfield: field in @_state (of type '@_arraytype *') which
 *               will point to the array
 * @_arrayprop: PropertyInfo defining what property the array elements have
 * @_arraytype: C type of the array elements
 *
 * Define device properties for a variable-length array _name.  A
 * static property "len-arrayname" is defined. When the device creator
 * sets this property to the desired length of array, further dynamic
 * properties "arrayname[0]", "arrayname[1]", ...  are defined so the
 * device creator can set the array element values. Setting the
 * "len-arrayname" property more than once is an error.
 *
 * When the array length is set, the @_field member of the device
 * struct is set to the array length, and @_arrayfield is set to point
 * to (zero-initialised) memory allocated for the array.  For a zero
 * length array, @_field will be set to 0 and @_arrayfield to NULL.
 * It is the responsibility of the device deinit code to free the
 * @_arrayfield memory.
 */
#define DEFINE_PROP_ARRAY(_name, _state, _field,                        \
                          _arrayfield, _arrayprop, _arraytype) {        \
        .name = (PROP_ARRAY_LEN_PREFIX _name),                          \
        .info = &(qdev_prop_arraylen),                                  \
        .set_default = true,                                            \
        .defval.u = 0,                                                  \
        .offset = offsetof(_state, _field)                              \
            + type_check(uint32_t, typeof_field(_state, _field)),       \
        .arrayinfo = &(_arrayprop),                                     \
        .arrayfieldsize = sizeof(_arraytype),                           \
        .arrayoffset = offsetof(_state, _arrayfield),                   \
        }

#define DEFINE_PROP_LINK(_name, _state, _field, _type, _ptr_type) {     \
        .name = (_name),                                                \
        .info = &(qdev_prop_link),                                      \
        .offset = offsetof(_state, _field)                              \
            + type_check(_ptr_type, typeof_field(_state, _field)),      \
        .link_type  = _type,                                            \
        }

#define DEFINE_PROP_UINT8(_n, _s, _f, _d)                       \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_uint8, uint8_t)
#define DEFINE_PROP_UINT16(_n, _s, _f, _d)                      \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_uint16, uint16_t)
#define DEFINE_PROP_UINT32(_n, _s, _f, _d)                      \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_uint32, uint32_t)
#define DEFINE_PROP_INT32(_n, _s, _f, _d)                      \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_int32, int32_t)
#define DEFINE_PROP_UINT64(_n, _s, _f, _d)                      \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_uint64, uint64_t)
#define DEFINE_PROP_INT64(_n, _s, _f, _d)                      \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_int64, int64_t)
#define DEFINE_PROP_SIZE(_n, _s, _f, _d)                       \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, _d, qdev_prop_size, uint64_t)
#define DEFINE_PROP_PCI_DEVFN(_n, _s, _f, _d)                   \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_pci_devfn, int32_t)

/*
 * Please avoid pointer properties.  If you must use them, you must
 * cover them in their device's class init function as follows:
 *
 * - If the property must be set, the device cannot be used with
 *   device_add, so add code like this:
 *   |* Reason: pointer property "NAME-OF-YOUR-PROP" *|
 *   DeviceClass *dc = DEVICE_CLASS(class);
 *   dc->user_creatable = false;
 *
 * - If the property may safely remain null, document it like this:
 *   |*
 *    * Note: pointer property "interrupt_vector" may remain null, thus
 *    * no need for dc->user_creatable = false;
 *    *|
 */
#define DEFINE_PROP_PTR(_n, _s, _f)             \
    DEFINE_PROP(_n, _s, _f, qdev_prop_ptr, void*)

#define DEFINE_PROP_CHR(_n, _s, _f)             \
    DEFINE_PROP(_n, _s, _f, qdev_prop_chr, CharBackend)
#define DEFINE_PROP_STRING(_n, _s, _f)             \
    DEFINE_PROP(_n, _s, _f, qdev_prop_string, char*)
#define DEFINE_PROP_NETDEV(_n, _s, _f)             \
    DEFINE_PROP(_n, _s, _f, qdev_prop_netdev, NICPeers)
#define DEFINE_PROP_VLAN(_n, _s, _f)             \
    DEFINE_PROP(_n, _s, _f, qdev_prop_vlan, NICPeers)
#define DEFINE_PROP_DRIVE(_n, _s, _f) \
    DEFINE_PROP(_n, _s, _f, qdev_prop_drive, BlockBackend *)
#define DEFINE_PROP_MACADDR(_n, _s, _f)         \
    DEFINE_PROP(_n, _s, _f, qdev_prop_macaddr, MACAddr)
#define DEFINE_PROP_ON_OFF_AUTO(_n, _s, _f, _d) \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_on_off_auto, OnOffAuto)
#define DEFINE_PROP_LOSTTICKPOLICY(_n, _s, _f, _d) \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_losttickpolicy, \
                        LostTickPolicy)
#define DEFINE_PROP_BLOCKDEV_ON_ERROR(_n, _s, _f, _d) \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_blockdev_on_error, \
                        BlockdevOnError)
#define DEFINE_PROP_BIOS_CHS_TRANS(_n, _s, _f, _d) \
    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_bios_chs_trans, int)
#define DEFINE_PROP_BLOCKSIZE(_n, _s, _f) \
    DEFINE_PROP_UNSIGNED(_n, _s, _f, 0, qdev_prop_blocksize, uint16_t)
#define DEFINE_PROP_PCI_HOST_DEVADDR(_n, _s, _f) \
    DEFINE_PROP(_n, _s, _f, qdev_prop_pci_host_devaddr, PCIHostDeviceAddress)
#define DEFINE_PROP_MEMORY_REGION(_n, _s, _f)             \
    DEFINE_PROP(_n, _s, _f, qdev_prop_ptr, MemoryRegion *)
#define DEFINE_PROP_UUID(_n, _s, _f) \
    DEFINE_PROP(_n, _s, _f, qdev_prop_uuid, QemuUUID)

#define DEFINE_PROP_END_OF_LIST()               \
    {}

/* Set properties between creation and init.  */
void *qdev_get_prop_ptr(DeviceState *dev, Property *prop);
void qdev_prop_set_bit(DeviceState *dev, const char *name, bool value);
void qdev_prop_set_uint8(DeviceState *dev, const char *name, uint8_t value);
void qdev_prop_set_uint16(DeviceState *dev, const char *name, uint16_t value);
void qdev_prop_set_uint32(DeviceState *dev, const char *name, uint32_t value);
void qdev_prop_set_int32(DeviceState *dev, const char *name, int32_t value);
void qdev_prop_set_uint64(DeviceState *dev, const char *name, uint64_t value);
void qdev_prop_set_string(DeviceState *dev, const char *name, const char *value);
void qdev_prop_set_chr(DeviceState *dev, const char *name, Chardev *value);
void qdev_prop_set_netdev(DeviceState *dev, const char *name, NetClientState *value);
void qdev_prop_set_drive(DeviceState *dev, const char *name,
                         BlockBackend *value, Error **errp);
void qdev_prop_set_macaddr(DeviceState *dev, const char *name,
                           const uint8_t *value);
void qdev_prop_set_enum(DeviceState *dev, const char *name, int value);
/* FIXME: Remove opaque pointer properties.  */
void qdev_prop_set_ptr(DeviceState *dev, const char *name, void *value);

void qdev_prop_register_global(GlobalProperty *prop);
void qdev_prop_register_global_list(GlobalProperty *props);
int qdev_prop_check_globals(void);
void qdev_prop_set_globals(DeviceState *dev);
void error_set_from_qdev_prop_error(Error **errp, int ret, DeviceState *dev,
                                    Property *prop, const char *value);

/**
 * register_compat_prop:
 *
 * Register internal (not user-provided) global property, changing the
 * default value of a given property in a device type.  This can be used
 * for enabling machine-type compatibility or for enabling
 * accelerator-specific defaults in devices.
 *
 * The property values set using this function must be always valid and
 * never report setter errors, as the property will have
 * GlobalProperty::errp set to &error_abort.
 *
 * User-provided global properties should override internal global
 * properties, so callers of this function should ensure that it is
 * called before user-provided global properties are registered.
 *
 * @driver: Device type to be affected
 * @property: Property whose default value is going to be changed
 * @value: New default value for the property
 */
void register_compat_prop(const char *driver, const char *property,
                          const char *value);
/*
 * register_compat_props_array(): using register_compat_prop(), which
 * only registers internal global properties (which has lower priority
 * than user-provided global properties)
 */
void register_compat_props_array(GlobalProperty *prop);

/**
 * qdev_property_add_static:
 * @dev: Device to add the property to.
 * @prop: The qdev property definition.
 * @errp: location to store error information.
 *
 * Add a static QOM property to @dev for qdev property @prop.
 * On error, store error in @errp.  Static properties access data in a struct.
 * The type of the QOM property is derived from prop->info.
 */
void qdev_property_add_static(DeviceState *dev, Property *prop, Error **errp);

void qdev_alias_all_properties(DeviceState *target, Object *source);

/**
 * @qdev_prop_set_after_realize:
 * @dev: device
 * @name: name of property
 * @errp: indirect pointer to Error to be set
 * Set the Error object to report that an attempt was made to set a property
 * on a device after it has already been realized. This is a utility function
 * which allows property-setter functions to easily report the error in
 * a friendly format identifying both the device and the property.
 */
void qdev_prop_set_after_realize(DeviceState *dev, const char *name,
                                 Error **errp);

/**
 * qdev_prop_allow_set_link_before_realize:
 *
 * Set the #Error object if an attempt is made to set the link after realize.
 * This function should be used as the check() argument to
 * object_property_add_link().
 */
void qdev_prop_allow_set_link_before_realize(const Object *obj,
                                             const char *name,
                                             Object *val, Error **errp);

#endif
