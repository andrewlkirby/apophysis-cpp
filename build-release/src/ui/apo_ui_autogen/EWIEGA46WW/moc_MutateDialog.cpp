/****************************************************************************
** Meta object code from reading C++ file 'MutateDialog.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/ui/MutateDialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MutateDialog.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMutateDialogENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMutateDialogENDCLASS = QtMocHelpers::stringData(
    "apo::ui::MutateDialog",
    "flameChanged",
    "",
    "renderRequested",
    "std::shared_ptr<const apo::Flame>",
    "flame",
    "seed",
    "onRenderFinished",
    "image",
    "pointsGenerated",
    "pointsAccepted",
    "onCellClicked",
    "cell",
    "onBack",
    "onTrendChanged",
    "index",
    "onStrengthChanged",
    "value",
    "onSameNumToggled",
    "checked",
    "onMaintainSymmetryToggled"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSapoSCOPEuiSCOPEMutateDialogENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   68,    2, 0x06,    1 /* Public */,
       3,    2,   69,    2, 0x06,    2 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    3,   74,    2, 0x08,    5 /* Private */,
      11,    1,   81,    2, 0x08,    9 /* Private */,
      13,    0,   84,    2, 0x08,   11 /* Private */,
      14,    1,   85,    2, 0x08,   12 /* Private */,
      16,    1,   88,    2, 0x08,   14 /* Private */,
      18,    1,   91,    2, 0x08,   16 /* Private */,
      20,    1,   94,    2, 0x08,   18 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4, QMetaType::ULongLong,    5,    6,

 // slots: parameters
    QMetaType::Void, QMetaType::QImage, QMetaType::ULongLong, QMetaType::ULongLong,    8,    9,   10,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   15,
    QMetaType::Void, QMetaType::Double,   17,
    QMetaType::Void, QMetaType::Bool,   19,
    QMetaType::Void, QMetaType::Bool,   19,

       0        // eod
};

Q_CONSTINIT const QMetaObject apo::ui::MutateDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMutateDialogENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSapoSCOPEuiSCOPEMutateDialogENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMutateDialogENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MutateDialog, std::true_type>,
        // method 'flameChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'renderRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<std::shared_ptr<const apo::Flame>, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        // method 'onRenderFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QImage, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        // method 'onCellClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onBack'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onTrendChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onStrengthChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'onSameNumToggled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onMaintainSymmetryToggled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void apo::ui::MutateDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MutateDialog *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->flameChanged(); break;
        case 1: _t->renderRequested((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const apo::Flame>>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2]))); break;
        case 2: _t->onRenderFinished((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[3]))); break;
        case 3: _t->onCellClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->onBack(); break;
        case 5: _t->onTrendChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onStrengthChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 7: _t->onSameNumToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->onMaintainSymmetryToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MutateDialog::*)();
            if (_t _q_method = &MutateDialog::flameChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (MutateDialog::*)(std::shared_ptr<const apo::Flame> , quint64 );
            if (_t _q_method = &MutateDialog::renderRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *apo::ui::MutateDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *apo::ui::MutateDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMutateDialogENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int apo::ui::MutateDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void apo::ui::MutateDialog::flameChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void apo::ui::MutateDialog::renderRequested(std::shared_ptr<const apo::Flame> _t1, quint64 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
