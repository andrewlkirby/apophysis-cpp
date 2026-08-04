/****************************************************************************
** Meta object code from reading C++ file 'PostProcessDialog.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/ui/PostProcessDialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PostProcessDialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSapoSCOPEuiSCOPEPostProcessDialogENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSapoSCOPEuiSCOPEPostProcessDialogENDCLASS = QtMocHelpers::stringData(
    "apo::ui::PostProcessDialog",
    "renderRequested",
    "",
    "std::shared_ptr<const apo::Flame>",
    "flame",
    "seed",
    "fullRenderRequested",
    "threadCount",
    "apo::RenderProgress*",
    "progress",
    "outputPath",
    "onRenderFinished",
    "image",
    "pointsGenerated",
    "pointsAccepted",
    "onFullRenderFinished",
    "cancelled",
    "saved",
    "onProgressTick"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSapoSCOPEuiSCOPEPostProcessDialogENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   44,    2, 0x06,    1 /* Public */,
       6,    5,   49,    2, 0x06,    4 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      11,    3,   60,    2, 0x08,   10 /* Private */,
      15,    5,   67,    2, 0x08,   14 /* Private */,
      18,    0,   78,    2, 0x08,   20 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, QMetaType::ULongLong,    4,    5,
    QMetaType::Void, 0x80000000 | 3, QMetaType::ULongLong, QMetaType::Int, 0x80000000 | 8, QMetaType::QString,    4,    5,    7,    9,   10,

 // slots: parameters
    QMetaType::Void, QMetaType::QImage, QMetaType::ULongLong, QMetaType::ULongLong,   12,   13,   14,
    QMetaType::Void, QMetaType::QImage, QMetaType::ULongLong, QMetaType::ULongLong, QMetaType::Bool, QMetaType::Bool,   12,   13,   14,   16,   17,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject apo::ui::PostProcessDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_CLASSapoSCOPEuiSCOPEPostProcessDialogENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSapoSCOPEuiSCOPEPostProcessDialogENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSapoSCOPEuiSCOPEPostProcessDialogENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<PostProcessDialog, std::true_type>,
        // method 'renderRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<std::shared_ptr<const apo::Flame>, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        // method 'fullRenderRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<std::shared_ptr<const apo::Flame>, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<apo::RenderProgress *, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'onRenderFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QImage, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        // method 'onFullRenderFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QImage, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onProgressTick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void apo::ui::PostProcessDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PostProcessDialog *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->renderRequested((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const apo::Flame>>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2]))); break;
        case 1: _t->fullRenderRequested((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const apo::Flame>>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<apo::RenderProgress*>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5]))); break;
        case 2: _t->onRenderFinished((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[3]))); break;
        case 3: _t->onFullRenderFinished((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[5]))); break;
        case 4: _t->onProgressTick(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (PostProcessDialog::*)(std::shared_ptr<const apo::Flame> , quint64 );
            if (_t _q_method = &PostProcessDialog::renderRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (PostProcessDialog::*)(std::shared_ptr<const apo::Flame> , quint64 , int , apo::RenderProgress * , QString );
            if (_t _q_method = &PostProcessDialog::fullRenderRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *apo::ui::PostProcessDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *apo::ui::PostProcessDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSapoSCOPEuiSCOPEPostProcessDialogENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int apo::ui::PostProcessDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void apo::ui::PostProcessDialog::renderRequested(std::shared_ptr<const apo::Flame> _t1, quint64 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void apo::ui::PostProcessDialog::fullRenderRequested(std::shared_ptr<const apo::Flame> _t1, quint64 _t2, int _t3, apo::RenderProgress * _t4, QString _t5)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t5))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
