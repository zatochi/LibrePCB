/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * http://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/
#include <QtCore>
#include <QtWidgets>
#include "addcomponentdialog.h"
#include "ui_addcomponentdialog.h"
#include <librepcb/common/graphics/graphicsscene.h>
#include <librepcb/common/graphics/graphicsview.h>
#include <librepcb/common/graphics/defaultgraphicslayerprovider.h>
#include <librepcb/project/project.h>
#include <librepcb/project/schematics/schematiclayerprovider.h>
#include <librepcb/project/library/projectlibrary.h>
#include <librepcb/library/cmp/component.h>
#include <librepcb/library/cmp/componentsymbolvariant.h>
#include <librepcb/library/dev/device.h>
#include <librepcb/library/pkg/package.h>
#include <librepcb/library/pkg/footprintpreviewgraphicsitem.h>
#include <librepcb/library/sym/symbol.h>
#include <librepcb/project/settings/projectsettings.h>
#include <librepcb/library/sym/symbolpreviewgraphicsitem.h>
#include <librepcb/workspace/workspace.h>
#include <librepcb/workspace/settings/workspacesettings.h>
#include <librepcb/workspace/library/cat/categorytreemodel.h>
#include <librepcb/workspace/workspace.h>
#include <librepcb/library/cat/componentcategory.h>
#include <librepcb/workspace/library/workspacelibrarydb.h>
#include <librepcb/common/gridproperties.h>
#include <librepcb/project/boards/board.h>
#include <librepcb/project/boards/boardlayerstack.h>

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

AddComponentDialog::AddComponentDialog(workspace::Workspace& workspace, Project& project,
                                       QWidget* parent) :
    QDialog(parent), mWorkspace(workspace), mProject(project),
    mUi(new Ui::AddComponentDialog), mComponentPreviewScene(nullptr),
    mDevicePreviewScene(nullptr), mCategoryTreeModel(nullptr),
    mSelectedComponent(nullptr), mSelectedSymbVar(nullptr), mSelectedDevice(nullptr),
    mSelectedPackage(nullptr), mPreviewFootprintGraphicsItem(nullptr)
{
    mUi->setupUi(this);
    mUi->treeComponents->setColumnCount(2);
    mUi->treeComponents->header()->setStretchLastSection(false);
    mUi->treeComponents->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mUi->treeComponents->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mUi->lblCompDescription->hide();
    mUi->lblSymbVar->hide();
    mUi->cbxSymbVar->hide();
    mUi->lblDeviceName->hide();
    mUi->viewDevice->hide();
    connect(mUi->edtSearch, &QLineEdit::textChanged,
            this, &AddComponentDialog::searchEditTextChanged);
    connect(mUi->treeComponents, &QTreeWidget::currentItemChanged,
            this, &AddComponentDialog::treeComponents_currentItemChanged);
    connect(mUi->treeComponents, &QTreeWidget::itemDoubleClicked,
            this, &AddComponentDialog::treeComponents_itemDoubleClicked);

    mComponentPreviewScene = new GraphicsScene();
    mUi->viewComponent->setScene(mComponentPreviewScene);
    mUi->viewComponent->setOriginCrossVisible(false);

    mDevicePreviewScene = new GraphicsScene();
    mUi->viewDevice->setScene(mDevicePreviewScene);
    mUi->viewDevice->setOriginCrossVisible(false);
    mUi->viewDevice->setBackgroundBrush(QBrush(Qt::black, Qt::SolidPattern));

    mGraphicsLayerProvider.reset(new DefaultGraphicsLayerProvider());

    const QStringList& localeOrder = mProject.getSettings().getLocaleOrder();
    mCategoryTreeModel = new workspace::ComponentCategoryTreeModel(mWorkspace.getLibraryDb(), localeOrder);
    mUi->treeCategories->setModel(mCategoryTreeModel);
    connect(mUi->treeCategories->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &AddComponentDialog::treeCategories_currentItemChanged);
}

AddComponentDialog::~AddComponentDialog() noexcept
{
    delete mPreviewFootprintGraphicsItem;       mPreviewFootprintGraphicsItem = nullptr;
    qDeleteAll(mPreviewSymbolGraphicsItems);    mPreviewSymbolGraphicsItems.clear();
    delete mSelectedPackage;                    mSelectedPackage = nullptr;
    delete mSelectedDevice;                     mSelectedDevice = nullptr;
    mSelectedSymbVar = nullptr;
    delete mSelectedComponent;                  mSelectedComponent = nullptr;
    delete mCategoryTreeModel;                  mCategoryTreeModel = nullptr;
    delete mDevicePreviewScene;                 mDevicePreviewScene = nullptr;
    delete mComponentPreviewScene;              mComponentPreviewScene = nullptr;
    delete mUi;                                 mUi = nullptr;
}

/*****************************************************************************************
 *  Getters
 ****************************************************************************************/

tl::optional<Uuid> AddComponentDialog::getSelectedComponentUuid() const noexcept
{
    if (mSelectedComponent && mSelectedSymbVar)
        return mSelectedComponent->getUuid();
    else
        return tl::nullopt;
}

tl::optional<Uuid> AddComponentDialog::getSelectedSymbVarUuid() const noexcept
{
    if (mSelectedComponent && mSelectedSymbVar)
        return mSelectedSymbVar->getUuid();
    else
        return tl::nullopt;
}

tl::optional<Uuid> AddComponentDialog::getSelectedDeviceUuid() const noexcept
{
    if (mSelectedComponent && mSelectedSymbVar && mSelectedDevice)
        return mSelectedDevice->getUuid();
    else
        return tl::nullopt;
}

/*****************************************************************************************
 *  Private Slots
 ****************************************************************************************/

void AddComponentDialog::searchEditTextChanged(const QString& text) noexcept
{
    try {
        QModelIndex catIndex = mUi->treeCategories->currentIndex();
        if (text.trimmed().isEmpty() && catIndex.isValid()) {
            setSelectedCategory(Uuid::tryFromString(catIndex.data(Qt::UserRole).toString()));
        } else {
            searchComponents(text.trimmed());
        }
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.getMsg());
    }
}

void AddComponentDialog::treeCategories_currentItemChanged(const QModelIndex& current,
                                                           const QModelIndex& previous) noexcept
{
    Q_UNUSED(previous);

    try {
        tl::optional<Uuid> categoryUuid = Uuid::tryFromString(current.data(Qt::UserRole).toString());
        setSelectedCategory(categoryUuid);
    } catch (Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.getMsg());
    }
}

void AddComponentDialog::treeComponents_currentItemChanged(QTreeWidgetItem *current,
                                                           QTreeWidgetItem *previous) noexcept
{
    Q_UNUSED(previous);
    try {
        if (current) {
            QTreeWidgetItem* cmpItem = current->parent() ? current->parent() : current;
            FilePath cmpFp = FilePath(cmpItem->data(0, Qt::UserRole).toString());
            if ((!mSelectedComponent) || (mSelectedComponent->getFilePath() != cmpFp)) {
                library::Component* component = new library::Component(cmpFp, true);
                setSelectedComponent(component);
            }
            if (current->parent()) {
                FilePath devFp = FilePath(current->data(0, Qt::UserRole).toString());
                if ((!mSelectedDevice) || (mSelectedDevice->getFilePath() != devFp)) {
                    library::Device* device = new library::Device(devFp, true);
                    setSelectedDevice(device);
                }
            } else {
                setSelectedDevice(nullptr);
            }
        } else {
            setSelectedComponent(nullptr);
        }
    } catch (Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.getMsg());
        setSelectedComponent(nullptr);
    }
}

void AddComponentDialog::treeComponents_itemDoubleClicked(QTreeWidgetItem* item, int column) noexcept
{
    Q_UNUSED(column);
    if (item && item->parent()) accept(); // only accept device items (not components)
}

void AddComponentDialog::on_cbxSymbVar_currentIndexChanged(int index) noexcept
{
    if ((mSelectedComponent) && (index >= 0)) {
        tl::optional<Uuid> uuid = Uuid::tryFromString(mUi->cbxSymbVar->itemData(index).toString());
        if (uuid) {
            setSelectedSymbVar(mSelectedComponent->getSymbolVariants().find(*uuid).get());
        } else {
            setSelectedSymbVar(nullptr);
        }
    } else {
        setSelectedSymbVar(nullptr);
    }
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void AddComponentDialog::searchComponents(const QString& input)
{
    setSelectedComponent(nullptr);
    mUi->treeComponents->clear();

    if (input.length() > 1) { // avoid freeze on entering first character due to huge result
        const QStringList& localeOrder = mProject.getSettings().getLocaleOrder();
        QSet<Uuid> components = mWorkspace.getLibraryDb().getComponentsBySearchKeyword(input);
        foreach (const Uuid& cmpUuid, components) {
            // component
            FilePath cmpFp = mWorkspace.getLibraryDb().getLatestComponent(cmpUuid);
            if (!cmpFp.isValid()) continue;
            QString cmpName;
            mWorkspace.getLibraryDb().getElementTranslations<library::Component>(cmpFp, localeOrder, &cmpName);
            QTreeWidgetItem* cmpItem = new QTreeWidgetItem(mUi->treeComponents);
            cmpItem->setText(0, cmpName);
            cmpItem->setData(0, Qt::UserRole, cmpFp.toStr());
            // devices
            QSet<Uuid> devices = mWorkspace.getLibraryDb().getDevicesOfComponent(cmpUuid);
            foreach (const Uuid& devUuid, devices) {
                try {
                    FilePath devFp = mWorkspace.getLibraryDb().getLatestDevice(devUuid);
                    if (!devFp.isValid()) continue;
                    QString devName;
                    mWorkspace.getLibraryDb().getElementTranslations<library::Device>(devFp, localeOrder, &devName);
                    QTreeWidgetItem* devItem = new QTreeWidgetItem(cmpItem);
                    devItem->setText(0, devName);
                    devItem->setData(0, Qt::UserRole, devFp.toStr());
                    // package
                    Uuid pkgUuid = Uuid::createRandom(); // only for initialization, will be overwritten
                    mWorkspace.getLibraryDb().getDeviceMetadata(devFp, &pkgUuid);
                    FilePath pkgFp = mWorkspace.getLibraryDb().getLatestPackage(pkgUuid);
                    if (pkgFp.isValid()) {
                        QString pkgName;
                        mWorkspace.getLibraryDb().getElementTranslations<library::Package>(pkgFp, localeOrder, &pkgName);
                        devItem->setText(1, pkgName);
                        devItem->setTextAlignment(1, Qt::AlignRight);
                    }
                } catch (const Exception& e) {
                    // what could we do here?
                }
            }
            cmpItem->setText(1, QString("[%1]").arg(devices.count()));
            cmpItem->setTextAlignment(1, Qt::AlignRight);
        }
    }

    mUi->treeComponents->sortByColumn(0, Qt::AscendingOrder);
}

void AddComponentDialog::setSelectedCategory(const tl::optional<Uuid>& categoryUuid)
{
    setSelectedComponent(nullptr);
    mUi->treeComponents->clear();

    const QStringList& localeOrder = mProject.getSettings().getLocaleOrder();

    mSelectedCategoryUuid = categoryUuid;
    QSet<Uuid> components = mWorkspace.getLibraryDb().getComponentsByCategory(categoryUuid);
    foreach (const Uuid& cmpUuid, components) {
        // component
        FilePath cmpFp = mWorkspace.getLibraryDb().getLatestComponent(cmpUuid);
        if (!cmpFp.isValid()) continue;
        QString cmpName;
        mWorkspace.getLibraryDb().getElementTranslations<library::Component>(cmpFp, localeOrder, &cmpName);
        QTreeWidgetItem* cmpItem = new QTreeWidgetItem(mUi->treeComponents);
        cmpItem->setText(0, cmpName);
        cmpItem->setData(0, Qt::UserRole, cmpFp.toStr());
        // devices
        QSet<Uuid> devices = mWorkspace.getLibraryDb().getDevicesOfComponent(cmpUuid);
        foreach (const Uuid& devUuid, devices) {
            try {
                FilePath devFp = mWorkspace.getLibraryDb().getLatestDevice(devUuid);
                if (!devFp.isValid()) continue;
                QString devName;
                mWorkspace.getLibraryDb().getElementTranslations<library::Device>(devFp, localeOrder, &devName);
                QTreeWidgetItem* devItem = new QTreeWidgetItem(cmpItem);
                devItem->setText(0, devName);
                devItem->setData(0, Qt::UserRole, devFp.toStr());
                // package
                Uuid pkgUuid = Uuid::createRandom(); // only for initialization, will be overwritten
                mWorkspace.getLibraryDb().getDeviceMetadata(devFp, &pkgUuid);
                FilePath pkgFp = mWorkspace.getLibraryDb().getLatestPackage(pkgUuid);
                if (pkgFp.isValid()) {
                    QString pkgName;
                    mWorkspace.getLibraryDb().getElementTranslations<library::Package>(pkgFp, localeOrder, &pkgName);
                    devItem->setText(1, pkgName);
                    devItem->setTextAlignment(1, Qt::AlignRight);
                }
            } catch (const Exception& e) {
                // what could we do here?
            }
        }
        cmpItem->setText(1, QString("[%1]").arg(devices.count()));
        cmpItem->setTextAlignment(1, Qt::AlignRight);
    }

    mUi->treeComponents->sortByColumn(0, Qt::AscendingOrder);
}

void AddComponentDialog::setSelectedComponent(const library::Component* cmp)
{
    if (cmp == mSelectedComponent) return;

    mUi->lblCompName->setText(tr("No component selected"));
    mUi->lblCompDescription->clear();
    setSelectedDevice(nullptr);
    setSelectedSymbVar(nullptr);
    delete mSelectedComponent;
    mSelectedComponent = nullptr;

    if (cmp)
    {
        const QStringList& localeOrder = mProject.getSettings().getLocaleOrder();

        mUi->lblCompName->setText(cmp->getNames().value(localeOrder));
        mUi->lblCompDescription->setText(cmp->getDescriptions().value(localeOrder));

        mSelectedComponent = cmp;

        mUi->cbxSymbVar->clear();
        for (const library::ComponentSymbolVariant& symbVar : cmp->getSymbolVariants()) {
            QString text = symbVar.getNames().value(localeOrder);
            if (!symbVar.getNorm().isEmpty()) {text += " [" + symbVar.getNorm() + "]";}
            mUi->cbxSymbVar->addItem(text, symbVar.getUuid().toStr());
        }
        mUi->cbxSymbVar->setCurrentIndex(cmp->getSymbolVariants().count() > 0 ? 0 : -1);
    }

    mUi->lblSymbVar->setVisible(mUi->cbxSymbVar->count() > 1);
    mUi->cbxSymbVar->setVisible(mUi->cbxSymbVar->count() > 1);
    mUi->lblCompDescription->setVisible(!mUi->lblCompDescription->text().isEmpty());
}

void AddComponentDialog::setSelectedSymbVar(const library::ComponentSymbolVariant* symbVar)
{
    if (symbVar == mSelectedSymbVar) return;
    qDeleteAll(mPreviewSymbolGraphicsItems);
    mPreviewSymbolGraphicsItems.clear();
    mSelectedSymbVar = symbVar;

    if (mSelectedComponent && symbVar) {
        const QStringList& localeOrder = mProject.getSettings().getLocaleOrder();
        for (const library::ComponentSymbolVariantItem& item : symbVar->getSymbolItems()) {
            FilePath symbolFp = mWorkspace.getLibraryDb().getLatestSymbol(item.getSymbolUuid());
            if (!symbolFp.isValid()) continue; // TODO: show warning
            const library::Symbol* symbol = new library::Symbol(symbolFp, true); // TODO: fix memory leak...
            library::SymbolPreviewGraphicsItem* graphicsItem = new library::SymbolPreviewGraphicsItem(
                *mGraphicsLayerProvider, localeOrder, *symbol, mSelectedComponent,
                symbVar->getUuid(), item.getUuid());
            graphicsItem->setPos(item.getSymbolPosition().toPxQPointF());
            graphicsItem->setRotation(-item.getSymbolRotation().toDeg());
            mPreviewSymbolGraphicsItems.append(graphicsItem);
            mComponentPreviewScene->addItem(*graphicsItem);
            mUi->viewComponent->zoomAll();
        }
    }
}

void AddComponentDialog::setSelectedDevice(const library::Device* dev)
{
    if (dev == mSelectedDevice) return;

    delete mPreviewFootprintGraphicsItem;   mPreviewFootprintGraphicsItem = nullptr;
    delete mSelectedPackage;                mSelectedPackage = nullptr;
    delete mSelectedDevice;                 mSelectedDevice = nullptr;

    if (dev) {
        mSelectedDevice = dev;
        const QStringList& localeOrder = mProject.getSettings().getLocaleOrder();
        FilePath pkgFp = mWorkspace.getLibraryDb().getLatestPackage(mSelectedDevice->getPackageUuid());
        if (pkgFp.isValid()) {
            mSelectedPackage = new library::Package(pkgFp, true);
            mUi->lblDeviceName->setText(QString("%1 [%2]").arg(
                mSelectedDevice->getNames().value(localeOrder),
                mSelectedPackage->getNames().value(localeOrder)));
            if (mSelectedPackage->getFootprints().count() > 0) {
                mPreviewFootprintGraphicsItem = new library::FootprintPreviewGraphicsItem(
                    *mGraphicsLayerProvider, localeOrder,
                    *mSelectedPackage->getFootprints().first(), mSelectedPackage,
                    mSelectedComponent);
                mDevicePreviewScene->addItem(*mPreviewFootprintGraphicsItem);
                mUi->viewDevice->zoomAll();
            }
        }
    }

    mUi->lblDeviceName->setVisible(dev ? true : false);
    mUi->viewDevice->setVisible(dev ? true : false);
    mUi->viewComponent->zoomAll();
}

void AddComponentDialog::accept() noexcept
{
    if ((!mSelectedComponent) || (!mSelectedSymbVar))
    {
        QMessageBox::information(this, tr("Invalid Selection"),
            tr("Please select a component and a symbol variant."));
        return;
    }

    QDialog::accept();
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace editor
} // namespace project
} // namespace librepcb
