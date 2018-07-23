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
#include "componentchooserdialog.h"
#include "ui_componentchooserdialog.h"
#include <librepcb/common/graphics/graphicsscene.h>
#include <librepcb/library/cmp/component.h>
#include <librepcb/library/sym/symbol.h>
#include <librepcb/library/sym/symbolpreviewgraphicsitem.h>
#include <librepcb/workspace/workspace.h>
#include <librepcb/workspace/settings/workspacesettings.h>
#include <librepcb/workspace/library/workspacelibrarydb.h>
#include <librepcb/workspace/library/cat/categorytreemodel.h>

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace library {
namespace editor {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

ComponentChooserDialog::ComponentChooserDialog(const workspace::Workspace& ws,
        const IF_GraphicsLayerProvider* layerProvider, QWidget* parent) noexcept :
    QDialog(parent), mWorkspace(ws), mLayerProvider(layerProvider),
    mUi(new Ui::ComponentChooserDialog)
{
    mUi->setupUi(this);
    mGraphicsScene.reset(new GraphicsScene());
    mUi->graphicsView->setScene(mGraphicsScene.data());

    mCategoryTreeModel.reset(new workspace::ComponentCategoryTreeModel(mWorkspace.getLibraryDb(),
                                                                       localeOrder()));
    mUi->treeCategories->setModel(mCategoryTreeModel.data());
    connect(mUi->treeCategories->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &ComponentChooserDialog::treeCategories_currentItemChanged);
    connect(mUi->listComponents, &QListWidget::currentItemChanged,
            this, &ComponentChooserDialog::listComponents_currentItemChanged);
    connect(mUi->listComponents, &QListWidget::itemDoubleClicked,
            this, &ComponentChooserDialog::listComponents_itemDoubleClicked);

    setSelectedComponent(tl::nullopt);
}

ComponentChooserDialog::~ComponentChooserDialog() noexcept
{
    setSelectedComponent(tl::nullopt);
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void ComponentChooserDialog::treeCategories_currentItemChanged(const QModelIndex& current,
                                                            const QModelIndex& previous) noexcept
{
    Q_UNUSED(previous);
    setSelectedCategory(Uuid::tryFromString(current.data(Qt::UserRole).toString()));
}

void ComponentChooserDialog::listComponents_currentItemChanged(QListWidgetItem* current,
                                                         QListWidgetItem* previous) noexcept
{
    Q_UNUSED(previous);
    if (current) {
        setSelectedComponent(Uuid::tryFromString(current->data(Qt::UserRole).toString()));
    } else {
        setSelectedComponent(tl::nullopt);
    }
}

void ComponentChooserDialog::listComponents_itemDoubleClicked(QListWidgetItem* item) noexcept
{
    if (item) {
        setSelectedComponent(Uuid::tryFromString(item->data(Qt::UserRole).toString()));
        accept();
    }
}

void ComponentChooserDialog::setSelectedCategory(const tl::optional<Uuid>& uuid) noexcept
{
    if (uuid && (uuid == mSelectedCategoryUuid)) return;

    setSelectedComponent(tl::nullopt);
    mUi->listComponents->clear();

    mSelectedCategoryUuid = uuid;
    try {
        QSet<Uuid> components = mWorkspace.getLibraryDb().getComponentsByCategory(uuid); // can throw
        foreach (const Uuid& uuid, components) {
            try {
                FilePath fp = mWorkspace.getLibraryDb().getLatestComponent(uuid); // can throw
                QString name;
                mWorkspace.getLibraryDb().getElementTranslations<Component>(fp, localeOrder(),
                                                                            &name); // can throw
                QListWidgetItem* item = new QListWidgetItem(name);
                item->setData(Qt::UserRole, uuid.toStr());
                mUi->listComponents->addItem(item);
            } catch (const Exception& e) {
                continue; // should we do something here?
            }
        }
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Could not load components"), e.getMsg());
    }
}

void ComponentChooserDialog::setSelectedComponent(const tl::optional<Uuid>& uuid) noexcept
{
    QString uuidStr = "00000000-0000-0000-0000-000000000000";
    QString name, desc;
    mSelectedComponentUuid = uuid;

    if (uuid) {
        try {
            uuidStr = uuid->toStr();
            mComponentFilePath = mWorkspace.getLibraryDb().getLatestComponent(*uuid); // can throw

            mWorkspace.getLibraryDb().getElementTranslations<Component>(
                mComponentFilePath, localeOrder(), &name, &desc); // can throw
        } catch (const Exception& e) {
            QMessageBox::critical(this, tr("Could not load component metadata"), e.getMsg());
        }
    }

    mUi->lblComponentUuid->setText(uuidStr);
    mUi->lblComponentName->setText(name);
    mUi->lblComponentDescription->setText(desc);
    updatePreview();
}

void ComponentChooserDialog::updatePreview() noexcept
{
    mSymbolGraphicsItems.clear();
    mSymbols.clear();
    mComponent.reset();

    if (mComponentFilePath.isValid() && mLayerProvider) {
        try {
            mComponent.reset(new Component(mComponentFilePath, true)); // can throw
            if (mComponent && mComponent->getSymbolVariants().count() > 0) {
                const ComponentSymbolVariant& symbVar = *mComponent->getSymbolVariants().first();
                for (const ComponentSymbolVariantItem& item : symbVar.getSymbolItems()) {
                    try {
                        FilePath fp = mWorkspace.getLibraryDb().getLatestSymbol(item.getSymbolUuid()); // can throw
                        std::shared_ptr<Symbol> sym = std::make_shared<Symbol>(fp, true); // can throw
                        mSymbols.append(sym);
                        std::shared_ptr<SymbolPreviewGraphicsItem> graphicsItem =
                            std::make_shared<SymbolPreviewGraphicsItem>(
                                    *mLayerProvider, QStringList(), *sym,
                                    mComponent.data(), symbVar.getUuid(), item.getUuid());
                        graphicsItem->setPos(item.getSymbolPosition().toPxQPointF());
                        graphicsItem->setRotation(-item.getSymbolRotation().toDeg());
                        mGraphicsScene->addItem(*graphicsItem);
                        mSymbolGraphicsItems.append(graphicsItem);
                    } catch (const Exception& e) {
                        // what could we do here? ;)
                    }
                }
                mUi->graphicsView->zoomAll();
            }
        } catch (const Exception& e) {
            // ignore errors...
        }
    }
}

void ComponentChooserDialog::accept() noexcept
{
    if (!mSelectedComponentUuid) {
        QMessageBox::information(this, tr("Invalid Selection"), tr("Please select a component."));
        return;
    }
    QDialog::accept();
}

const QStringList& ComponentChooserDialog::localeOrder() const noexcept
{
    return mWorkspace.getSettings().getLibLocaleOrder().getLocaleOrder();
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace editor
} // namespace library
} // namespace librepcb
