/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/ui/MainWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMainWindowENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMainWindowENDCLASS = QtMocHelpers::stringData(
    "apo::ui::MainWindow",
    "renderRequested",
    "",
    "std::shared_ptr<const apo::Flame>",
    "flame",
    "seed",
    "apo::RenderProgress*",
    "progress",
    "onOpenTriggered",
    "onSaveRenderTriggered",
    "onRenderFinished",
    "image",
    "pointsGenerated",
    "pointsAccepted",
    "onSelectionChanged",
    "row",
    "onThumbnailFinished",
    "index",
    "onItemActivated",
    "QListWidgetItem*",
    "item",
    "openOptionsDialog",
    "openAboutDialog",
    "onNewFlameTriggered",
    "onSaveFlameAsTriggered",
    "onSaveAllFlamesTriggered",
    "onCopyFlameTriggered",
    "onPasteFlameTriggered",
    "onUndo",
    "onRedo",
    "onRenderAllFlamesTriggered",
    "onSmoothPaletteTriggered",
    "onNewRandomBatchTriggered",
    "onEditFlameTriggered",
    "onDuplicateFlameTriggered",
    "onRenameFlameTriggered",
    "onDeleteFlameTriggered",
    "onResetLocationTriggered",
    "onFlameListContextMenuRequested",
    "pos",
    "onFlameItemChanged",
    "onCameraGestureStarted",
    "onCameraChanged",
    "onCameraChangeFinished",
    "onProgressTick",
    "onViewModeChanged",
    "thumbnails",
    "onQualityBoxCommitted"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSapoSCOPEuiSCOPEMainWindowENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      32,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    3,  206,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       8,    0,  213,    2, 0x08,    5 /* Private */,
       9,    0,  214,    2, 0x08,    6 /* Private */,
      10,    3,  215,    2, 0x08,    7 /* Private */,
      14,    1,  222,    2, 0x08,   11 /* Private */,
      16,    2,  225,    2, 0x08,   13 /* Private */,
      18,    1,  230,    2, 0x08,   16 /* Private */,
      21,    0,  233,    2, 0x08,   18 /* Private */,
      22,    0,  234,    2, 0x08,   19 /* Private */,
      23,    0,  235,    2, 0x08,   20 /* Private */,
      24,    0,  236,    2, 0x08,   21 /* Private */,
      25,    0,  237,    2, 0x08,   22 /* Private */,
      26,    0,  238,    2, 0x08,   23 /* Private */,
      27,    0,  239,    2, 0x08,   24 /* Private */,
      28,    0,  240,    2, 0x08,   25 /* Private */,
      29,    0,  241,    2, 0x08,   26 /* Private */,
      30,    0,  242,    2, 0x08,   27 /* Private */,
      31,    0,  243,    2, 0x08,   28 /* Private */,
      32,    0,  244,    2, 0x08,   29 /* Private */,
      33,    0,  245,    2, 0x08,   30 /* Private */,
      34,    0,  246,    2, 0x08,   31 /* Private */,
      35,    0,  247,    2, 0x08,   32 /* Private */,
      36,    0,  248,    2, 0x08,   33 /* Private */,
      37,    0,  249,    2, 0x08,   34 /* Private */,
      38,    1,  250,    2, 0x08,   35 /* Private */,
      40,    1,  253,    2, 0x08,   37 /* Private */,
      41,    0,  256,    2, 0x08,   39 /* Private */,
      42,    0,  257,    2, 0x08,   40 /* Private */,
      43,    0,  258,    2, 0x08,   41 /* Private */,
      44,    0,  259,    2, 0x08,   42 /* Private */,
      45,    1,  260,    2, 0x08,   43 /* Private */,
      47,    0,  263,    2, 0x08,   45 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, QMetaType::ULongLong, 0x80000000 | 6,    4,    5,    7,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QImage, QMetaType::ULongLong, QMetaType::ULongLong,   11,   12,   13,
    QMetaType::Void, QMetaType::Int,   15,
    QMetaType::Void, QMetaType::Int, QMetaType::QImage,   17,   11,
    QMetaType::Void, 0x80000000 | 19,   20,
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
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QPoint,   39,
    QMetaType::Void, 0x80000000 | 19,   20,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   46,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject apo::ui::MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMainWindowENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSapoSCOPEuiSCOPEMainWindowENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMainWindowENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'renderRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<std::shared_ptr<const apo::Flame>, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<apo::RenderProgress *, std::false_type>,
        // method 'onOpenTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSaveRenderTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRenderFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QImage, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        QtPrivate::TypeAndForceComplete<quint64, std::false_type>,
        // method 'onSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onThumbnailFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<QImage, std::false_type>,
        // method 'onItemActivated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QListWidgetItem *, std::false_type>,
        // method 'openOptionsDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'openAboutDialog'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNewFlameTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSaveFlameAsTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSaveAllFlamesTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCopyFlameTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPasteFlameTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onUndo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRedo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRenderAllFlamesTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSmoothPaletteTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNewRandomBatchTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onEditFlameTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDuplicateFlameTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRenameFlameTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeleteFlameTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onResetLocationTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onFlameListContextMenuRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPoint &, std::false_type>,
        // method 'onFlameItemChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QListWidgetItem *, std::false_type>,
        // method 'onCameraGestureStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCameraChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCameraChangeFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onProgressTick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onViewModeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onQualityBoxCommitted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void apo::ui::MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->renderRequested((*reinterpret_cast< std::add_pointer_t<std::shared_ptr<const apo::Flame>>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<apo::RenderProgress*>>(_a[3]))); break;
        case 1: _t->onOpenTriggered(); break;
        case 2: _t->onSaveRenderTriggered(); break;
        case 3: _t->onRenderFinished((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<quint64>>(_a[3]))); break;
        case 4: _t->onSelectionChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->onThumbnailFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QImage>>(_a[2]))); break;
        case 6: _t->onItemActivated((*reinterpret_cast< std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        case 7: _t->openOptionsDialog(); break;
        case 8: _t->openAboutDialog(); break;
        case 9: _t->onNewFlameTriggered(); break;
        case 10: _t->onSaveFlameAsTriggered(); break;
        case 11: _t->onSaveAllFlamesTriggered(); break;
        case 12: _t->onCopyFlameTriggered(); break;
        case 13: _t->onPasteFlameTriggered(); break;
        case 14: _t->onUndo(); break;
        case 15: _t->onRedo(); break;
        case 16: _t->onRenderAllFlamesTriggered(); break;
        case 17: _t->onSmoothPaletteTriggered(); break;
        case 18: _t->onNewRandomBatchTriggered(); break;
        case 19: _t->onEditFlameTriggered(); break;
        case 20: _t->onDuplicateFlameTriggered(); break;
        case 21: _t->onRenameFlameTriggered(); break;
        case 22: _t->onDeleteFlameTriggered(); break;
        case 23: _t->onResetLocationTriggered(); break;
        case 24: _t->onFlameListContextMenuRequested((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 25: _t->onFlameItemChanged((*reinterpret_cast< std::add_pointer_t<QListWidgetItem*>>(_a[1]))); break;
        case 26: _t->onCameraGestureStarted(); break;
        case 27: _t->onCameraChanged(); break;
        case 28: _t->onCameraChangeFinished(); break;
        case 29: _t->onProgressTick(); break;
        case 30: _t->onViewModeChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 31: _t->onQualityBoxCommitted(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MainWindow::*)(std::shared_ptr<const apo::Flame> , quint64 , apo::RenderProgress * );
            if (_t _q_method = &MainWindow::renderRequested; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject *apo::ui::MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *apo::ui::MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSapoSCOPEuiSCOPEMainWindowENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int apo::ui::MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 32)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 32;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 32)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 32;
    }
    return _id;
}

// SIGNAL 0
void apo::ui::MainWindow::renderRequested(std::shared_ptr<const apo::Flame> _t1, quint64 _t2, apo::RenderProgress * _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
