#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/SettingsManager.h"

SettingsDialog::SettingsDialog(SettingsManager *settingsManager, QWidget *parent)
    : QDialog(parent)
    , m_settingsManager(settingsManager)
{
    // Give the dialog a simple native title and a practical fixed starting size.
    setWindowTitle(QStringLiteral("clip-stacker Settings"));
    resize(420, 160);

    auto *mainLayout = new QVBoxLayout(this);
    auto *descriptionLabel = new QLabel(QStringLiteral("Configure clipboard retention."), this);
    auto *formLayout = new QFormLayout();

    // Edit the retained clipboard history size with a sensible bound for desktop usage.
    m_historyLimitSpinBox = new QSpinBox(this);
    m_historyLimitSpinBox->setRange(25, 5000);
    formLayout->addRow(QStringLiteral("History limit"), m_historyLimitSpinBox);

    // Add the form controls and dialog buttons to the layout.
    mainLayout->addWidget(descriptionLabel);
    mainLayout->addLayout(formLayout);
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // Connect the standard dialog buttons.
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    // Populate the form from the current persistent settings.
    loadSettings();
}

void SettingsDialog::saveSettings()
{
    SettingsManager::SettingsSnapshot snapshot = m_settingsManager->snapshot();

    // Persist the updated user-facing retention value.
    snapshot.historyLimit = m_historyLimitSpinBox->value();
    m_settingsManager->setSnapshot(snapshot);
    accept();
}

void SettingsDialog::loadSettings()
{
    const SettingsManager::SettingsSnapshot snapshot = m_settingsManager->snapshot();

    // Reflect the currently persisted settings so the dialog always opens with real values.
    m_historyLimitSpinBox->setValue(snapshot.historyLimit);
}
