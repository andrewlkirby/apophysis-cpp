/****************************************************************************
** Meta object code from reading C++ file 'EditorWindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/ui/EditorWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'EditorWindow.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSapoSCOPEuiSCOPEEditorWindowENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSapoSCOPEuiSCOPEEditorWindowENDCLASS = QtMocHelpers::stringData(
    "apo::ui::EditorWindow",
    "renderRequested",
    "",
    "std::shared_ptr<const apo::Flame>",
    "flame",
    "seed",
    "apo::RenderProgress*",
    "progress",
    "onXformEdited",
    "index",
    "onEditingStarted",
    "onEditingFinished",
    "onRenderFinished",
    "image",
    "pointsGenerated",
    "pointsAccepted",
    "onXformListSelectionChanged",
    "row",
    "onCanvasSelectionChanged",
    "onUndo",
    "onRedo",
    "openRenderDialog",
    "openMutateDialog",
    "onSaveFlameAsTriggered",
    "openFullscreenView",
    "openCurvesDialog",
    "onAddXform",
    "onDuplicateXform",
    "onDeleteXform",
    "onCopyXform",
    "onPasteXform",
    "openXaosDialog",
    "openForceSymmetryDialog",
    "onForceSymmetryRequested",
    "sym",
    "onXformPropertyEditingStarted",
    "onXformPropertyEditingFinished",
    "onFinalXformToggled",
    "enabled",
    "onRandomizeWeights",
    "onEqualizeWeights",
    "onCalculateColorValues",
    "onRandomizeColorValues",
    "onQualityBoxCommitted",
    "onProgressTick",
    "onDescriptionsVisibilityChanged",
    "show",
    "onRightPanelTabChanged"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSapoSCOPEuiSCOPEEditorWindowENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      33,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    3,  212,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       8,    1,  219,    2, 0x08,    5 /* Private */,
      10,    1,  222,    2, 0x08,    7 /* Private */,
      11,    1,  225,    2, 0x08,    9 /* Private */,
      12,    3,  228,    2, 0x08,   11 /* Private */,
      16,    1,  235,    2, 0x08,   15 /* Private */,
      18,    1,  238,    2, 0x08,   17 /* Private */,
      19,    0,  241,    2, 0x08,   19 /* Private */,
      20,    0,  242,    2, 0x08,   20 /* Private */,
      21,    0,  243,    2, 0x08,   21 /* Private */,
      22,    0,  244,    2, 0x08,   22 /* Private */,
      23,    0,  245,    2, 0x08,   23 /* Private */,
      24,    0,  246,    2, 0x08,   24 /* Private */,
      25,    0,  247,    2, 0x08,   25 /* Private */,
      26,    0,  248,    2, 0x08,   26 /* Private */,
      27,    0,  249,    2, 0x08,   27 /* Private */,
      28,    0,  250,    2, 0x08,   28 /* Private */,
      29,    0,  251,    2, 0x08,   29 /* Private */,
      30,    0,  252,    2, 0x08,   30 /* Private */,
      31,    0,  253,    2, 0x08,   31 /* Private */,
      32,    0,  254,    2, 0x08,   32 /* Private */,
      33,    1,  255,    2, 0x08,   33 /* Private */,
      35,    0,  258,    2, 0x08,   35 /* Private */,
      36,    0,  259,    2, 0x08,   36 /* Private */,
      37,    1,  260,    2, 0x08,   37 /* Private */,
      39,    0,  263,    2, 0x08,   39 /* Private */,
      40,    0,  264,    2, 0x08,   40 /* Private */,
      41,    0,  265,    2, 0x08,   41 /* Private */,
      42,    0,  266,    2, 0x08,   42 /* Private */,
      43,    0,  267,    2, 0x08,   43 /* Private */,
      44,    0,  268,    2, 0x08,   44 /* Private */,
      45,    1,  269,    2, 0x08,   45 /* Private */,
      47,    1,  272,    2, 0x08,   47 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, QMetaType::ULongLong, 0x80000000 | 6,    4,    5,    7,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::QImage, QMetaType::ULongLong, QMetaType::ULongLong,   13,   14,   15,
    QMetaType::Void, QMetaType::Int,   17,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   34,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   38,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   46,
    QMetaType::Void, QMetaType::Int,    9,

       0        // eod
};

Q_CONSTINIT const QMetaObject apo::ui::EditorWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_CLASSapoSCOPEuiSCOPEEditorWindowENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSapoSCOPEuiSCOPEEditorWindowENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSapoSCOPEuiSCOPEEditorWindowENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<EditorWindow, std::true_type>,
        // method 'renderRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<std::shared_ptr<const apo::Flame>, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<apo::RenderProgress *, std::false_type>,
        // method 'onXformEdited'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onEditingStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onEditingFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onRenderFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QImage, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        // method 'onXformListSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onCanvasSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onUndo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRedo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openRenderDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openMutateDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSaveFlameAsTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openFullscreenView'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openCurvesDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onAddXform'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDuplicateXform'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeleteXform'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCopyXform'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPasteXform'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openXaosDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openForceSymmetryDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onForceSymmetryRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onXformPropertyEditingStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onXformPropertyEditingFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onFinalXformToggled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onRandomizeWeights'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onEqualizeWeights'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCalculateColorValues'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRandomizeColorValues'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onQualityBoxCommitted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onProgressTick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDescriptionsVisibilityChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onRightPanelTabChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void apo::ui::EditorWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<EditorWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->renderRequested((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const apo::Flame>>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<apo::RenderProgress*>>(_a[3]))); break;
        case 1: _t->onXformEdited((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->onEditingStarted((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->onEditingFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->onRenderFinished((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[3]))); break;
        case 5: _t->onXformListSelectionChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onCanvasSelectionChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->onUndo(); break;
        case 8: _t->onRedo(); break;
        case 9: _t->openRenderDialog(); break;
        case 10: _t->openMutateDialog(); break;
        case 11: _t->onSaveFlameAsTriggered(); break;
        case 12: _t->openFullscreenView(); break;
        case 13: _t->openCurvesDialog(); break;
        case 14: _t->onAddXform(); break;
        case 15: _t->onDuplicateXform(); break;
        case 16: _t->onDeleteXform(); break;
        case 17: _t->onCopyXform(); break;
        case 18: _t->onPasteXform(); break;
        case 19: _t->openXaosDialog(); break;
        case 20: _t->openForceSymmetryDialog(); break;
        case 21: _t->onForceSymmetryRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 22: _t->onXformPropertyEditingStarted(); break;
        case 23: _t->onXformPropertyEditingFinished(); break;
        case 24: _t->onFinalXformToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 25: _t->onRandomizeWeights(); break;
        case 26: _t->onEqualizeWeights(); break;
        case 27: _t->onCalculateColorValues(); break;
        case 28: _t->onRandomizeColorValues(); break;
        case 29: _t->onQualityBoxCommitted(); break;
        case 30: _t->onProgressTick(); break;
        case 31: _t->onDescriptionsVisibilityChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 32: _t->onRightPanelTabChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (EditorWindow::*)(std::shared_ptr<const apo::Flame> , quint64 , apo::RenderProgress * );
            if (_t _q_method = &EditorWindow::renderRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *apo::ui::EditorWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *apo::ui::EditorWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSapoSCOPEuiSCOPEEditorWindowENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int apo::ui::EditorWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 33)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 33;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 33)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 33;
    }
    return _id;
}

// SIGNAL 0
void apo::ui::EditorWindow::renderRequested(std::shared_ptr<const apo::Flame> _t1, quint64 _t2, apo::RenderProgress * _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
