/****************************************************************************
** Meta object code from reading C++ file 'RenderDialog.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/ui/RenderDialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'RenderDialog.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSapoSCOPEuiSCOPERenderDialogENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSapoSCOPEuiSCOPERenderDialogENDCLASS = QtMocHelpers::stringData(
    "apo::ui::RenderDialog",
    "fullRenderRequested",
    "",
    "std::shared_ptr<const apo::Flame>",
    "flame",
    "seed",
    "threadCount",
    "apo::RenderProgress*",
    "progress",
    "outputPath",
    "onFullRenderFinished",
    "image",
    "pointsGenerated",
    "pointsAccepted",
    "cancelled",
    "saved",
    "usedGpu",
    "onProgressTick",
    "browseOutputPath",
    "startRender",
    "cancelRender",
    "togglePauseRender",
    "openPostProcess"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSapoSCOPEuiSCOPERenderDialogENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    5,   62,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      10,    6,   73,    2, 0x08,    7 /* Private */,
      17,    0,   86,    2, 0x08,   14 /* Private */,
      18,    0,   87,    2, 0x08,   15 /* Private */,
      19,    0,   88,    2, 0x08,   16 /* Private */,
      20,    0,   89,    2, 0x08,   17 /* Private */,
      21,    0,   90,    2, 0x08,   18 /* Private */,
      22,    0,   91,    2, 0x08,   19 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, QMetaType::ULongLong, QMetaType::Int, 0x80000000 | 7, QMetaType::QString,    4,    5,    6,    8,    9,

 // slots: parameters
    QMetaType::Void, QMetaType::QImage, QMetaType::ULongLong, QMetaType::ULongLong, QMetaType::Bool, QMetaType::Bool, QMetaType::Bool,   11,   12,   13,   14,   15,   16,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject apo::ui::RenderDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_CLASSapoSCOPEuiSCOPERenderDialogENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSapoSCOPEuiSCOPERenderDialogENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSapoSCOPEuiSCOPERenderDialogENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<RenderDialog, std::true_type>,
        // method 'fullRenderRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<std::shared_ptr<const apo::Flame>, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<apo::RenderProgress *, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'onFullRenderFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QImage, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onProgressTick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'browseOutputPath'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'startRender'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cancelRender'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'togglePauseRender'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openPostProcess'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void apo::ui::RenderDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<RenderDialog *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->fullRenderRequested((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const apo::Flame>>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<apo::RenderProgress*>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[5]))); break;
        case 1: _t->onFullRenderFinished((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[6]))); break;
        case 2: _t->onProgressTick(); break;
        case 3: _t->browseOutputPath(); break;
        case 4: _t->startRender(); break;
        case 5: _t->cancelRender(); break;
        case 6: _t->togglePauseRender(); break;
        case 7: _t->openPostProcess(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (RenderDialog::*)(std::shared_ptr<const apo::Flame> , quint64 , int , apo::RenderProgress * , QString );
            if (_t _q_method = &RenderDialog::fullRenderRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *apo::ui::RenderDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *apo::ui::RenderDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSapoSCOPEuiSCOPERenderDialogENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int apo::ui::RenderDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void apo::ui::RenderDialog::fullRenderRequested(std::shared_ptr<const apo::Flame> _t1, quint64 _t2, int _t3, apo::RenderProgress * _t4, QString _t5)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t5))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
