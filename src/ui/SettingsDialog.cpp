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
    // Give the dialog a simple native title sized for the history limit and hotkey label rows.
    setWindowTitle(QStringLiteral("clip-stacker Settings"));
    resize(440, 200);

    auto *mainLayout = new QVBoxLayout(this);
    auto *descriptionLabel = new QLabel(QStringLiteral("Configure clipboard retention settings."), this);
    auto *formLayout = new QFormLayout();

    // Constrain the history limit to 1–500: rejects negatives, zero, and non-numeric input naturally.
    m_historyLimitSpinBox = new QSpinBox(this);
    m_historyLimitSpinBox->setRange(1, 500);          // Max 500 prevents excessive memory use
    m_historyLimitSpinBox->setSuffix(QStringLiteral(" items"));
    m_historyLimitSpinBox->setToolTip(QStringLiteral("Number of clipboard entries to keep (1–500)."));
    formLayout->addRow(QStringLiteral("History limit"), m_historyLimitSpinBox);

    // Show the fixed popup hotkey as a read-only label so users know the shortcut.
    auto *hotkeyLabel = new QLabel(QStringLiteral("Ctrl + Alt + V"), this);
    hotkeyLabel->setToolTip(QStringLiteral("Fixed global shortcut to open the clipboard history popup."));
    formLayout->addRow(QStringLiteral("Popup hotkey"), hotkeyLabel);

    // Assemble the layout: description, form rows, then action buttons.
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

    // Clamp to the declared range before persisting — guards against programmatic out-of-range values.
    snapshot.historyLimit = qBound(1, m_historyLimitSpinBox->value(), 500);

    m_settingsManager->setSnapshot(snapshot);
    accept();
}

void SettingsDialog::loadSettings()
{
    const SettingsManager::SettingsSnapshot snapshot = m_settingsManager->snapshot();

    // Reflect the persisted limit, clamped so out-of-range legacy values still render correctly.
    m_historyLimitSpinBox->setValue(qBound(1, snapshot.historyLimit, 500));
}
