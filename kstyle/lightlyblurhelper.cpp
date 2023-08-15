//////////////////////////////////////////////////////////////////////////////
// lightlyblurhelper.cpp
// handle regions passed to kwin for blurring
// -------------------
//
// Copyright (C) 2018 Alex Nemeth <alex.nemeth329@gmail.com>
//
// Largely rewritten from Oxygen widget style
// Copyright (C) 2007 Thomas Luebking <thomas.luebking@web.de>
// Copyright (c) 2010 Hugo Pereira Da Costa <hugo.pereira@free.fr>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include "lightlyblurhelper.h"
#include "lightlypropertynames.h"
#include "lightlystyleconfigdata.h"

#include <KWindowEffects>

#include <QEvent>
#include <QMainWindow>
#include <QMenu>
#include <QPair>
#include <QRegularExpression>
#include <QToolBar>
#include <QVector>
#include <QPainterPath>
//#include <QDebug>
namespace
{

    QRegion roundedRegion(const QRect &rect, int radius, bool topLeft, bool topRight, bool bottomLeft, bool bottomRight)
    {
        QPainterPath regionPath;
        regionPath.addRect(rect.x(), rect.y() + radius, rect.width(), rect.height() - 2 * radius);

        regionPath.addRect(
            rect.x() + topLeft * radius,
            rect.y(),
            rect.width() - topLeft * radius - topRight * radius,
            radius
        );
        regionPath.addRect(
            rect.x() + bottomLeft * radius,
            rect.y() + rect.height() - radius,
            rect.width() - bottomLeft * radius - bottomRight * radius,
            radius
        );

        if (topLeft) {
            QPainterPath ellipse;
            ellipse.addEllipse(rect.x(), rect.y(), 2 * radius, 2 * radius);
            regionPath |= ellipse;
        }

        if (topRight) {
            QPainterPath ellipse;
            ellipse.addEllipse(rect.x() + rect.width() - 2 * radius, rect.y(), 2 * radius, 2 * radius);
            regionPath |= ellipse;
        }

        if (bottomRight) {
            QPainterPath ellipse;
            ellipse.addEllipse(rect.x() + rect.width() - 2 * radius, rect.y() + rect.height() - 2 * radius, 2 * radius, 2 * radius);
            regionPath |= ellipse;
        }

        if (bottomLeft) {
            QPainterPath ellipse;
            ellipse.addEllipse(rect.x(), rect.y() + rect.height() - 2 * radius, 2 * radius, 2 * radius);
            regionPath |= ellipse;
        }

        return QRegion(regionPath.toFillPolygon().toPolygon());
    }

}

namespace Lightly
{
    //___________________________________________________________
    BlurHelper::BlurHelper(QObject* parent):
        QObject(parent)
    {
    }

    //___________________________________________________________
    void BlurHelper::registerWidget(QWidget* widget, const bool isDolphin)
    {
        // install event filter
        addEventFilter(widget);

        // schedule shadow area repaint
        update(widget);

        _isDolphin = isDolphin;
    }

    //___________________________________________________________
    void BlurHelper::unregisterWidget(QWidget* widget)
    {
        // remove event filter
        widget->removeEventFilter(this);
    }

    //___________________________________________________________
    bool BlurHelper::eventFilter(QObject* object, QEvent* event)
    {

        switch (event->type()) {
            case QEvent::Hide:
            case QEvent::Show:
            case QEvent::Resize:
            {
                // cast to widget and check
                QWidget* widget(qobject_cast<QWidget*>(object));

                if (!widget)
                    break;

                update(widget);
                break;
            }

            default: break;
        }

        // never eat events
        return false;
    }

    //___________________________________________________________
    QRegion BlurHelper::blurRegion (QWidget* widget) const
    {
        if (!widget->isVisible()) return QRegion();

        QRect rect = widget->rect();
        QRegion wMask = widget->mask();

        /* blurring may not be suitable when the available
            painting area is restricted by a widget mask */
        if (!wMask.isEmpty() && wMask != QRegion(rect))
            return QRegion();

        else if ((qobject_cast<QMenu*>(widget)
            && !widget->testAttribute(Qt::WA_X11NetWmWindowTypeMenu)) // not a detached menu
            || widget->inherits("QComboBoxPrivateContainer"))
        {
            return roundedRegion(rect, StyleConfigData::cornerRadius()+1, true, true, true, true);
        }
        else
        {
            // blur entire window
            if( widget->palette().color( QPalette::Window ).alpha() < 255 )
                return rect;

            // blur specific widgets
            QRegion region;

            // toolbar and menubar
            if( _translucentTitlebar )
            {
                // menubar
                int menubarHeight = 0;
                if ( QMainWindow *mw = qobject_cast<QMainWindow*>( widget ) )
                {
                    if ( QWidget *mb = mw->menuWidget() )
                    {
                        if ( mb->isVisible() )
                        {
                            region += mb->rect();
                            menubarHeight = mb->height();
                        }
                    }
                }

                QList<QToolBar *> toolbars = widget->window()->findChildren<QToolBar *>( QString(), Qt::FindDirectChildrenOnly );

                for( auto tb : toolbars )
                {
                    // find all horizontal toolbars touching the header
                    if (tb && tb->isVisible()
                        && (tb->orientation() == Qt::Horizontal || _isDolphin)
                        && (tb->y() == 0 || tb->y() == menubarHeight))
                    {
                        region += QRegion( QRect( tb->pos(), tb->rect().size() ) );
                    }
                }
            }

            // dolphin's sidebar
            if( StyleConfigData::dolphinSidebarOpacity() < 100 && _isDolphin )
            {
                // sidetoolbar
                if( !_translucentTitlebar )
                {
                    QToolBar *toolbar = widget->window()->findChild<QToolBar *>( QString(), Qt::FindDirectChildrenOnly );
                    if( toolbar && toolbar->orientation() == Qt::Vertical ) {
                        region += QRect( toolbar->pos(), toolbar->rect().size() );
                    }
                }

                // sidepanels
                QList<QWidget *> sidebars = widget->findChildren<QWidget *>( QRegularExpression("^(places|terminal|info|folders)Dock$"), Qt::FindDirectChildrenOnly );
                for ( auto sb : sidebars )
                {
                    if ( sb && sb->isVisible() )
                    {
                        region += QRect( sb->pos(), sb->rect().size() );
                    }
                }
            }

            return region;
        }
    }

    //___________________________________________________________
    void BlurHelper::update(QWidget* widget) const
    {
        /*
        directly from bespin code. Supposedly prevent playing with some 'pseudo-widgets'
        that have winId matching some other -random- window
        */
        if (!(widget->testAttribute(Qt::WA_WState_Created) || widget->internalWinId()))
            return;

        QRegion region = blurRegion(widget);
        if (region.isNull()) return;

        KWindowEffects::enableBlurBehind(widget->isWindow() ? widget->windowHandle() : widget->window()->windowHandle(), true, region);
        //KWindowEffects::enableBackgroundContrast (widget->isWindow() ? widget->winId() : widget->window()->winId(), true, 1.0, 1.2, 1.3, region );

        // force update
        if (widget->isVisible()) {
            widget->update();
        }
    }
}
