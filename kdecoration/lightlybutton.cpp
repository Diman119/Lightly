/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 * Copyright 2014  Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "lightlybutton.h"

#include <KDecoration2/DecoratedClient>
#include <KColorUtils>
#include <KIconLoader>

#include <QPainter>
#include <QVariantAnimation>
#include <QPainterPath>

namespace Lightly
{

    using KDecoration2::ColorRole;
    using KDecoration2::ColorGroup;
    using KDecoration2::DecorationButtonType;


    //__________________________________________________________________
    Button::Button(DecorationButtonType type, Decoration* decoration, QObject* parent)
        : DecorationButton(type, decoration, parent)
        , m_animation( new QVariantAnimation( this ) )
    {

        // setup animation
        // It is important start and end value are of the same type, hence 0.0 and not just 0
        m_animation->setStartValue( 0.0 );
        m_animation->setEndValue( 1.0 );
        m_animation->setEasingCurve( QEasingCurve::InOutQuad );
        connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            setOpacity(value.toReal());
        });

        // setup default geometry
        const int height = decoration->buttonHeight();
        setGeometry(QRect(0, 0, height, height));
        setIconSize(QSize( height, height ));

        // connections
        connect(decoration->client().data(), SIGNAL(iconChanged(QIcon)), this, SLOT(update()));
        connect(decoration->settings().data(), &KDecoration2::DecorationSettings::reconfigured, this, &Button::reconfigure);
        connect( this, &KDecoration2::DecorationButton::hoveredChanged, this, &Button::updateAnimationState );

        reconfigure();

    }

    //__________________________________________________________________
    Button::Button(QObject *parent, const QVariantList &args)
        : Button(args.at(0).value<DecorationButtonType>(), args.at(1).value<Decoration*>(), parent)
    {
        m_flag = FlagStandalone;
        //! icon size must return to !valid because it was altered from the default constructor,
        //! in Standalone mode the button is not using the decoration metrics but its geometry
        m_iconSize = QSize(-1, -1);
    }

    //__________________________________________________________________
    Button *Button::create(DecorationButtonType type, KDecoration2::Decoration *decoration, QObject *parent)
    {
        if (auto d = qobject_cast<Decoration*>(decoration))
        {
            Button *b = new Button(type, d, parent);
            switch( type )
            {

                case DecorationButtonType::Close:
                b->setVisible( d->client().data()->isCloseable() );
                QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::closeableChanged, b, &Lightly::Button::setVisible );
                break;

                case DecorationButtonType::Maximize:
                b->setVisible( d->client().data()->isMaximizeable() );
                QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::maximizeableChanged, b, &Lightly::Button::setVisible );
                break;

                case DecorationButtonType::Minimize:
                b->setVisible( d->client().data()->isMinimizeable() );
                QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::minimizeableChanged, b, &Lightly::Button::setVisible );
                break;

                case DecorationButtonType::ContextHelp:
                b->setVisible( d->client().data()->providesContextHelp() );
                QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::providesContextHelpChanged, b, &Lightly::Button::setVisible );
                break;

                case DecorationButtonType::Shade:
                b->setVisible( d->client().data()->isShadeable() );
                QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::shadeableChanged, b, &Lightly::Button::setVisible );
                break;

                case DecorationButtonType::Menu:
                QObject::connect(d->client().data(), &KDecoration2::DecoratedClient::iconChanged, b, [b]() { b->update(); });
                break;

                default: break;

            }

            return b;
        }

        return nullptr;

    }

    //__________________________________________________________________
    void Button::paint(QPainter *painter, const QRect &repaintRegion)
    {
        Q_UNUSED(repaintRegion)

        if (!decoration()) return;

        painter->save();

        if( !m_iconSize.isValid() ) m_iconSize = geometry().size().toSize();

        drawIcon( painter );

        painter->restore();

    }

    //__________________________________________________________________
    void Button::drawIcon( QPainter *painter ) const
    {
        auto d = qobject_cast<Decoration*>(decoration());

        painter->setRenderHints( QPainter::Antialiasing );

        painter->save();
        painter->translate(geometry().topLeft());

        const qreal width(geometry().width());
        const qreal height(geometry().height());

        // render background
        const QColor backgroundColor(this->backgroundColor());
        if (backgroundColor.isValid()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(backgroundColor);

            QRectF backgrRect(0, 0, width, height);
            const int radius = d->internalSettings()->windowCornerRadius();

            // round top corner of first and last buttons
            if (m_flag == FlagFirstInList && !d->isLeftEdge() && !d->isTopEdge()) {

                painter->setClipRect(backgrRect, Qt::IntersectClip);
                backgrRect.adjust(0, 0, radius, radius);  // clipping extra rounded corners
                painter->drawRoundedRect(backgrRect, radius, radius);

            } else if (m_flag == FlagLastInList && !d->isRightEdge() && !d->isTopEdge()) {

                painter->setClipRect(backgrRect, Qt::IntersectClip);
                backgrRect.adjust(-radius, 0, 0, radius);
                painter->drawRoundedRect(backgrRect, radius, radius);

            } else {

                painter->drawRect(backgrRect);

            }
        }
        painter->restore();

        if (!d->isTopEdge()) {
            painter->translate(0, 1);
        }

        if (type() == DecorationButtonType::Menu) {  // render app icon

            const QRectF iconRect(geometry().topLeft() + m_offset, m_iconSize);
            if (auto deco =  qobject_cast<Decoration*>(decoration())) {
                const QPalette activePalette = KIconLoader::global()->customPalette();
                QPalette palette = decoration()->client().data()->palette();
                palette.setColor(QPalette::Foreground, deco->fontColor());
                KIconLoader::global()->setCustomPalette(palette);
                decoration()->client().data()->icon().paint(painter, iconRect.toRect());
                if (activePalette == QPalette()) {
                    KIconLoader::global()->resetPalette();
                }    else {
                    KIconLoader::global()->setCustomPalette(palette);
                }
            } else {
                decoration()->client().data()->icon().paint(painter, iconRect.toRect());
            }

        } else {  // render mark

            const QColor foregroundColor(this->foregroundColor());
            if (foregroundColor.isValid()) {
                // setup painter
                painter->translate(geometry().center());
                painter->scale(height * 0.17, height * 0.17);
                QPen pen(foregroundColor);
                pen.setCapStyle(Qt::RoundCap);
                pen.setJoinStyle(Qt::RoundJoin);
                pen.setWidthF(PenWidth::Symbol / height * 8.0);

                painter->setPen(pen);
                painter->setBrush(Qt::NoBrush);

                switch (type()) {

                    case DecorationButtonType::Close:
                        painter->drawLine(QPointF(-1, -1), QPointF(1, 1));
                        painter->drawLine(QPointF(1, -1), QPointF(-1, 1));
                        break;

                    case DecorationButtonType::Maximize:
                        if (isChecked()) {
                            painter->drawRoundedRect(QRectF(-1, -0.5, 1.5, 1.5), 0.3, 0.3);
                            painter->setClipRect(QRectF(-0.45, -1, 1.45, 1.45), Qt::ReplaceClip);
                            painter->drawRoundedRect(QRectF(-1, -1, 2.0, 2.0), 0.5, 0.5);
                            painter->setClipRect(QRectF(0, 0, 0, 0), Qt::NoClip);
                        } else {
                            painter->drawRoundedRect(QRectF(-1, -1, 2, 2), 0.3, 0.3);
                        }
                        break;

                    case DecorationButtonType::Minimize:
                        painter->drawLine(QPointF(-1, 0), QPointF(1, 0));
                        break;

                    case DecorationButtonType::ApplicationMenu:
                        painter->drawLine(QPointF(-1, -1), QPointF(1, -1));
                        painter->drawLine(QPointF(-1, 0), QPointF(1, 0));
                        painter->drawLine(QPointF(-1, 1), QPointF(1, 1));
                        break;

                    // TODO: the rest of the buttons
                }
            }
        }
    }

    //__________________________________________________________________
    QColor Button::foregroundColor() const {
        auto d = qobject_cast<Decoration*>(decoration());
        if (!d) {
            return QColor();
        }
        return d->fontColor();
    }

    //__________________________________________________________________
    QColor Button::backgroundColor() const {
        auto d = qobject_cast<Decoration*>(decoration());
        if (!d) {
            return QColor();
        }

        auto c = d->client().data();

        if (type() == DecorationButtonType::Close) {
            if (isPressed()) {
                return c->color(ColorGroup::Warning, ColorRole::Foreground).lighter();
            } else if (m_animation->state() == QAbstractAnimation::Running) {
                QColor color(c->color(ColorGroup::Warning, ColorRole::Foreground));
                color.setAlpha(color.alpha() * m_opacity);
                return color;
            } else if (isHovered()) {
                return c->color(ColorGroup::Warning, ColorRole::Foreground);
            }
        }

        auto color = d->fontColor();

        if (isPressed() || ((
                                type() == DecorationButtonType::KeepBelow ||
                                type() == DecorationButtonType::KeepAbove ||
                                type() == DecorationButtonType::Shade) && isChecked())
            ) {
            color.setAlpha(64);
            return color;
        } else if (m_animation->state() == QAbstractAnimation::Running) {
            color.setAlpha(32 * m_opacity);
            return color;
        } else if (isHovered()) {
            color.setAlpha(32);
            return color;
        }

        return QColor();
    }

    //________________________________________________________________
    void Button::reconfigure()
    {

        // animation
        auto d = qobject_cast<Decoration*>(decoration());
        if( d )  m_animation->setDuration( d->internalSettings()->animationsDuration() );

    }

    //__________________________________________________________________
    void Button::updateAnimationState( bool hovered )
    {

        auto d = qobject_cast<Decoration*>(decoration());
        if( !(d && d->internalSettings()->animationsEnabled() ) ) return;

        m_animation->setDirection( hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward );
        if( m_animation->state() != QAbstractAnimation::Running ) m_animation->start();

    }

} // namespace
