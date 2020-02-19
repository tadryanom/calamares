/*
 *   Copyright 2016, Luca Giambonini <almack@chakraos.org>
 *   Copyright 2016, Lisa Vitolo <shainer@chakraos.org>
 *   Copyright 2017, Kyle Robbertze  <krobbertze@gmail.com>
 *   Copyright 2017-2018, 2020, Adriaan de Groot <groot@kde.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */

#include "NetInstallViewStep.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include "utils/Logger.h"
#include "utils/Variant.h"

#include "NetInstallPage.h"

CALAMARES_PLUGIN_FACTORY_DEFINITION( NetInstallViewStepFactory, registerPlugin< NetInstallViewStep >(); )

NetInstallViewStep::NetInstallViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_widget( new NetInstallPage() )
    , m_nextEnabled( false )
    , m_sidebarLabel( nullptr )
{
    emit nextStatusChanged( true );
    connect( m_widget, &NetInstallPage::checkReady, this, &NetInstallViewStep::nextIsReady );
}


NetInstallViewStep::~NetInstallViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
    delete m_sidebarLabel;
}


QString
NetInstallViewStep::prettyName() const
{
    return m_sidebarLabel ? m_sidebarLabel->get() : tr( "Package selection" );
}


QWidget*
NetInstallViewStep::widget()
{
    return m_widget;
}


bool
NetInstallViewStep::isNextEnabled() const
{
    return m_nextEnabled;
}


bool
NetInstallViewStep::isBackEnabled() const
{
    return true;
}


bool
NetInstallViewStep::isAtBeginning() const
{
    return true;
}


bool
NetInstallViewStep::isAtEnd() const
{
    return true;
}


QList< Calamares::job_ptr >
NetInstallViewStep::jobs() const
{
    return m_jobs;
}


void
NetInstallViewStep::onActivate()
{
    m_widget->onActivate();
}

void
NetInstallViewStep::onLeave()
{
    PackageModel::PackageItemDataList packages = m_widget->selectedPackages();
    cDebug() << "Netinstall: Processing" << packages.length() << "packages.";

    static const char PACKAGEOP[] = "packageOperations";

    // Check if there's already a PACAKGEOP entry in GS, and if so we'll
    // extend that one (overwriting the value in GS at the end of this method)
    Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
    QVariantList packageOperations = gs->contains( PACKAGEOP ) ? gs->value( PACKAGEOP ).toList() : QVariantList();
    cDebug() << Logger::SubEntry << "Existing package operations length" << packageOperations.length();

    // Clear out existing operations for this module, going backwards:
    // Sometimes we remove an item, and we don't want the index to
    // fall off the end of the list.
    for ( int index = packageOperations.length() - 1; 0 <= index; index-- )
    {
        const QVariantMap op = packageOperations.at( index ).toMap();
        if ( op.contains( "source" ) && op.value( "source" ).toString() == moduleInstanceKey().toString() )
        {
            cDebug() << Logger::SubEntry << "Removing existing operations for" << moduleInstanceKey();
            packageOperations.removeAt( index );
        }
    }

    // This netinstall module may add two sub-steps to the packageOperations,
    // one for installing and one for try-installing.
    QVariantList installPackages;
    QVariantList tryInstallPackages;

    for ( const auto& package : packages )
    {
        if ( package.isCritical )
        {
            installPackages.append( package.toOperation() );
        }
        else
        {
            tryInstallPackages.append( package.toOperation() );
        }
    }

    if ( !installPackages.empty() )
    {
        QVariantMap op;
        op.insert( "install", QVariant( installPackages ) );
        op.insert( "source", moduleInstanceKey().toString() );
        packageOperations.append( op );
        cDebug() << Logger::SubEntry << installPackages.length() << "critical packages.";
    }
    if ( !tryInstallPackages.empty() )
    {
        QVariantMap op;
        op.insert( "try_install", QVariant( tryInstallPackages ) );
        op.insert( "source", moduleInstanceKey().toString() );
        packageOperations.append( op );
        cDebug() << Logger::SubEntry << tryInstallPackages.length() << "non-critical packages.";
    }

    if ( !packageOperations.isEmpty() )
    {
        gs->insert( PACKAGEOP, packageOperations );
    }
}

void
NetInstallViewStep::nextIsReady( bool b )
{
    m_nextEnabled = b;
    emit nextStatusChanged( b );
}

void
NetInstallViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_widget->setRequired( CalamaresUtils::getBool( configurationMap, "required", false ) );

    QString groupsUrl = CalamaresUtils::getString( configurationMap, "groupsUrl" );
    if ( !groupsUrl.isEmpty() )
    {
        // Keep putting groupsUrl into the global storage,
        // even though it's no longer used for in-module data-passing.
        Calamares::JobQueue::instance()->globalStorage()->insert( "groupsUrl", groupsUrl );
        m_widget->loadGroupList( groupsUrl );
    }

    bool bogus = false;
    auto label = CalamaresUtils::getSubMap( configurationMap, "label", bogus );

    if ( label.contains( "sidebar" ) )
    {
        m_sidebarLabel = new CalamaresUtils::Locale::TranslatedString( label, "sidebar", metaObject()->className() );
    }
    if ( label.contains( "title" ) )
    {
        m_widget->setPageTitle( new CalamaresUtils::Locale::TranslatedString( label, "title", metaObject()->className() ) );
    }
}
