#include "ActionList.hpp"

#include <State/Address.hpp>

#include <Process/Dataflow/AddressAccessorEditWidget.hpp>

#include <StreamDeck/Editor/ButtonEditor.hpp>
#include <StreamDeck/Editor/ColorButton.hpp>
#include <StreamDeck/Editor/WheelGuard.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::ActionList)
W_OBJECT_IMPL(StreamDeck::FeedbackList)
W_OBJECT_IMPL(StreamDeck::OverrideEditor)

namespace StreamDeck
{
namespace
{
QFrame* rowFrame(QWidget* parent)
{
  auto f = new QFrame{parent};
  f->setFrameShape(QFrame::StyledPanel);
  return f;
}

QPushButton* smallButton(const QString& text, const QString& tip, QWidget* parent)
{
  auto b = new QPushButton{text, parent};
  b->setToolTip(tip);
  const int h = b->fontMetrics().height() + 8;
  b->setFixedSize(h + 4, h);
  return b;
}

QLineEdit* deckNameEdit(const QString& prefix, const QString& name, QWidget* parent)
{
  auto e = new QLineEdit{parent};
  e->setText(name);
  e->setPlaceholderText(QObject::tr("name"));
  e->setToolTip(QObject::tr(
      "The address is created in the deck's tree, at %1<name>: map it to anything, "
      "differently along the score.").arg(prefix));
  return e;
}

Process::AddressAccessorEditWidget*
addressEditor(const score::DocumentContext& ctx, const QString& addr, QWidget* parent)
{
  auto w = new Process::AddressAccessorEditWidget{ctx, parent};
  if(auto a = State::parseAddressAccessor(addr))
    w->setAddress(*a);
  return w;
}

void clearLayout(QVBoxLayout* l)
{
  while(auto item = l->takeAt(0))
  {
    if(auto w = item->widget())
    {
      w->blockSignals(true);
      for(auto child : w->findChildren<QObject*>())
        child->blockSignals(true);
      w->hide();
      w->deleteLater();
    }
    delete item;
  }
}

const char* const action_labels[] = {
    QT_TRANSLATE_NOOP("StreamDeck", "Send value"),
    QT_TRANSLATE_NOOP("StreamDeck", "Toggle"),
    QT_TRANSLATE_NOOP("StreamDeck", "Step"),
    QT_TRANSLATE_NOOP("StreamDeck", "Go to page"),
    QT_TRANSLATE_NOOP("StreamDeck", "Next page"),
    QT_TRANSLATE_NOOP("StreamDeck", "Previous page"),
    QT_TRANSLATE_NOOP("StreamDeck", "Script"),
    QT_TRANSLATE_NOOP("StreamDeck", "Trigger"),
};

const char* const condition_labels[] = {
    QT_TRANSLATE_NOOP("StreamDeck", "is on"),
    QT_TRANSLATE_NOOP("StreamDeck", "is off"),
    QT_TRANSLATE_NOOP("StreamDeck", "="),
    QT_TRANSLATE_NOOP("StreamDeck", "≠"),
    QT_TRANSLATE_NOOP("StreamDeck", ">"),
    QT_TRANSLATE_NOOP("StreamDeck", "<"),
    QT_TRANSLATE_NOOP("StreamDeck", "script is true"),
};

TextField* codeEditor(const QString& code, const QString& placeholder, QWidget* parent)
{
  auto t = new TextField{parent};
  t->setTabChangesFocus(true);
  auto f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  t->setFont(f);
  t->setFixedHeight(QFontMetrics{f}.lineSpacing() * 4 + 10);
  t->setPlaceholderText(placeholder);
  t->setPlainText(code);
  return t;
}
}

ActionList::ActionList(const score::DocumentContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_ctx{ctx}
{
  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(2);
  m_rows = new QVBoxLayout;
  m_rows->setSpacing(2);
  lay->addLayout(m_rows);

  auto add = new QPushButton{tr("Add an action"), this};
  auto menu = new QMenu{add};
  auto addTrigger = [this](bool toggle) {
    m_actions.push_back({.type = Action::Trigger, .address = m_defaultName, .toggle = toggle});
    rebuild();
    commit();
  };
  menu->addAction(tr("Trigger: an impulse at an address of the deck"), this, [=] {
    addTrigger(false);
  });
  menu->addAction(tr("Trigger: an on / off address of the deck"), this, [=] {
    addTrigger(true);
  });
  menu->addSeparator();
  for(int t = 0; t <= Action::Script; t++)
  {
    menu->addAction(tr(action_labels[t]), this, [this, t] {
      m_actions.push_back({.type = Action::Type(t)});
      rebuild();
      commit();
    });
  }
  menu->addSeparator();
  auto transport = [this](QString label, QString addr, ossia::value v) {
    return std::make_pair(label, Action{.type = Action::Send, .address = addr, .value = v});
  };
  for(auto [label, action] :
      {transport(tr("Play"), "score:/play", true),
       transport(tr("Pause"), "score:/play", false),
       transport(tr("Stop"), "score:/stop", ossia::impulse{}),
       transport(tr("Back to start"), "score:/transport", 0.f)})
  {
    menu->addAction(label, this, [this, action] {
      m_actions.push_back(action);
      rebuild();
      commit();
    });
  }
  add->setMenu(menu);
  lay->addWidget(add, 0, Qt::AlignLeft);
}

void ActionList::setActions(
    std::vector<Action> actions, QStringList pages, QString defaultName)
{
  m_defaultName = std::move(defaultName);
  if(actions == m_actions && pages == m_pages)
    return;
  m_actions = std::move(actions);
  m_pages = std::move(pages);
  rebuild();
}

void ActionList::commit()
{
  changed();
}

void ActionList::rebuild()
{
  clearLayout(m_rows);
  for(int i = 0; i < int(m_actions.size()); i++)
    m_rows->addWidget(makeRow(i));
}

QWidget* ActionList::makeRow(int i)
{
  const Action& a = m_actions[i];
  auto row = rowFrame(this);
  auto grid = new QGridLayout{row};
  grid->setContentsMargins(4, 2, 2, 2);
  grid->setHorizontalSpacing(4);
  grid->setVerticalSpacing(2);

  auto type = new QComboBox{row};
  for(auto label : action_labels)
    type->addItem(tr(label));
  type->setCurrentIndex(a.type);
  connect(type, &QComboBox::currentIndexChanged, this, [this, i](int t) {
    m_actions[i].type = Action::Type(t);
    QTimer::singleShot(0, this, [this] {
      rebuild();
      commit();
    });
  });
  grid->addWidget(type, 0, 0);

  auto delay = new QSpinBox{row};
  delay->setRange(0, 600000);
  delay->setSingleStep(100);
  delay->setSuffix(tr(" ms"));
  delay->setPrefix(tr("after "));
  delay->setSpecialValueText(tr("at once"));
  delay->setKeyboardTracking(false);
  delay->setValue(a.delay);
  delay->setToolTip(tr("Delay before the action runs"));
  connect(delay, &QSpinBox::valueChanged, this, [this, i](int v) {
    m_actions[i].delay = v;
    commit();
  });
  grid->addWidget(delay, 0, 1);

  auto buttons = new QHBoxLayout;
  buttons->setSpacing(0);
  auto up = smallButton(QStringLiteral("▲"), tr("Move up"), row);
  auto down = smallButton(QStringLiteral("▼"), tr("Move down"), row);
  auto del = smallButton(QStringLiteral("✕"), tr("Remove"), row);
  up->setEnabled(i > 0);
  down->setEnabled(i + 1 < int(m_actions.size()));
  connect(up, &QPushButton::clicked, this, [this, i] {
    std::swap(m_actions[i], m_actions[i - 1]);
    QTimer::singleShot(0, this, [this] {
      rebuild();
      commit();
    });
  });
  connect(down, &QPushButton::clicked, this, [this, i] {
    std::swap(m_actions[i], m_actions[i + 1]);
    QTimer::singleShot(0, this, [this] {
      rebuild();
      commit();
    });
  });
  connect(del, &QPushButton::clicked, this, [this, i] {
    m_actions.erase(m_actions.begin() + i);
    QTimer::singleShot(0, this, [this] {
      rebuild();
      commit();
    });
  });
  buttons->addWidget(up);
  buttons->addWidget(down);
  buttons->addWidget(del);
  grid->addLayout(buttons, 0, 2);

  int line = 1;
  if(a.needsAddress())
  {
    auto addr = addressEditor(m_ctx, a.address, row);
    connect(
        addr, &Process::AddressAccessorEditWidget::addressChanged, this,
        [this, i, addr] {
      m_actions[i].address = addr->addressString();
      commit();
    });
    grid->addWidget(addr, line++, 0, 1, 3);
  }

  switch(a.type)
  {
    case Action::Send: {
      auto value = new QLineEdit{row};
      value->setPlaceholderText(tr("Value: 1, 0.5, true, \"text\", [1, 2]…"));
      value->setText(valueToString(a.value));
      connect(value, &QLineEdit::editingFinished, this, [this, i, value] {
        auto v = valueFromString(value->text());
        if(v == m_actions[i].value)
          return;
        m_actions[i].value = v;
        commit();
      });
      grid->addWidget(value, line++, 0, 1, 3);
      break;
    }
    case Action::Step: {
      auto step = new QDoubleSpinBox{row};
      step->setRange(-1e6, 1e6);
      step->setDecimals(3);
      step->setPrefix(tr("by "));
      step->setKeyboardTracking(false);
      step->setValue(a.step);
      connect(step, &QDoubleSpinBox::valueChanged, this, [this, i](double v) {
        m_actions[i].step = v;
        commit();
      });
      grid->addWidget(step, line++, 0, 1, 3);
      break;
    }
    case Action::Trigger: {
      auto lay = new QHBoxLayout;
      lay->addWidget(new QLabel{QStringLiteral("deck:/actions/"), row});
      auto name = deckNameEdit(QStringLiteral("deck:/actions/"), a.address, row);
      lay->addWidget(name, 1);
      auto mode = new QComboBox{row};
      mode->addItem(tr("Impulse"));
      mode->addItem(tr("On / off"));
      mode->setToolTip(
          tr("Impulse: each press sends an impulse.\nOn / off: each press switches a "
             "boolean, and the key looks pressed while it is on."));
      mode->setCurrentIndex(a.toggle ? 1 : 0);
      lay->addWidget(mode);
      connect(name, &QLineEdit::editingFinished, this, [this, i, name] {
        const auto path = deckPath(name->text());
        if(name->text() != path)
          name->setText(path);
        if(m_actions[i].address == path)
          return;
        m_actions[i].address = path;
        commit();
      });
      connect(mode, &QComboBox::currentIndexChanged, this, [this, i](int m) {
        m_actions[i].toggle = m == 1;
        commit();
      });
      grid->addLayout(lay, line++, 0, 1, 3);
      break;
    }
    case Action::Script: {
      auto code = codeEditor(
          a.script,
          tr("JavaScript, run in the console's engine. For instance:\n"
             "Device.write(\"osc:/light\", 1);\n"
             "if(deck.page() == 1) deck.nextPage();"),
          row);
      connect(
          code, &TextField::editingFinished, this,
          [this, i, code] {
        if(m_actions[i].script == code->toPlainText())
          return;
        m_actions[i].script = code->toPlainText();
        commit();
      });
      grid->addWidget(code, line++, 0, 1, 3);
      break;
    }
    case Action::GotoPage: {
      auto page = new QComboBox{row};
      page->addItems(m_pages);
      page->setCurrentIndex(std::clamp(a.page, 0, int(m_pages.size()) - 1));
      connect(page, &QComboBox::currentIndexChanged, this, [this, i](int p) {
        m_actions[i].page = p;
        commit();
      });
      grid->addWidget(page, line++, 0, 1, 3);
      break;
    }
    default:
      break;
  }
  grid->setColumnStretch(0, 1);
  protectFromWheel(row);
  return row;
}

FeedbackList::FeedbackList(const score::DocumentContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_ctx{ctx}
{
  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(2);
  m_rows = new QVBoxLayout;
  m_rows->setSpacing(2);
  lay->addLayout(m_rows);

  auto add = new QPushButton{tr("Add a feedback"), this};
  connect(add, &QPushButton::clicked, this, [this] {
    m_feedbacks.push_back({.style = {.background = QColor{0x2f, 0x7d, 0x32}}});
    rebuild();
    commit();
  });
  lay->addWidget(add, 0, Qt::AlignLeft);
}

void FeedbackList::setFeedbacks(std::vector<Feedback> f, QString defaultName)
{
  m_defaultName = std::move(defaultName);
  if(f == m_feedbacks)
    return;
  m_feedbacks = std::move(f);
  rebuild();
}

void FeedbackList::commit()
{
  changed();
}

void FeedbackList::rebuild()
{
  clearLayout(m_rows);
  for(int i = 0; i < int(m_feedbacks.size()); i++)
    m_rows->addWidget(makeRow(i));
}

QWidget* FeedbackList::makeRow(int i)
{
  const Feedback& f = m_feedbacks[i];
  auto row = rowFrame(this);
  auto grid = new QGridLayout{row};
  grid->setContentsMargins(4, 4, 4, 4);
  grid->setHorizontalSpacing(4);
  grid->setVerticalSpacing(4);

  auto source = new QComboBox{row};
  source->addItem(tr("While an address of the score"));
  source->addItem(tr("While an input of the deck"));
  source->addItem(tr("While a script"));
  source->setToolTip(
      tr("An address of the score: any address of the Device Explorer.\n"
         "An input of the deck: an address created in the deck's tree, at "
         "deck:/feedbacks/<name>, made to be mapped to along the score.\n"
         "A script: a JavaScript expression."));
  source->setCurrentIndex(!f.needsAddress() ? 2 : f.deckInput ? 1 : 0);
  grid->addWidget(source, 0, 0, 1, 2);

  auto del = smallButton(QStringLiteral("✕"), tr("Remove"), row);
  connect(del, &QPushButton::clicked, this, [this, i] {
    m_feedbacks.erase(m_feedbacks.begin() + i);
    QTimer::singleShot(0, this, [this] {
      rebuild();
      commit();
    });
  });
  grid->addWidget(del, 0, 2);

  switch(source->currentIndex())
  {
    case 0: {
      auto addr = addressEditor(m_ctx, f.address, row);
      connect(
          addr, &Process::AddressAccessorEditWidget::addressChanged, this,
          [this, i, addr] {
        m_feedbacks[i].address = addr->addressString();
        commit();
      });
      grid->addWidget(addr, 1, 0, 1, 3);
      break;
    }
    case 1: {
      auto lay = new QHBoxLayout;
      lay->addWidget(new QLabel{QStringLiteral("deck:/feedbacks/"), row});
      auto name = deckNameEdit(QStringLiteral("deck:/feedbacks/"), f.address, row);
      lay->addWidget(name, 1);
      connect(name, &QLineEdit::editingFinished, this, [this, i, name] {
        const auto path = deckPath(name->text());
        if(name->text() != path)
          name->setText(path);
        if(m_feedbacks[i].address == path)
          return;
        m_feedbacks[i].address = path;
        commit();
      });
      grid->addLayout(lay, 1, 0, 1, 3);
      break;
    }
    case 2: {
      auto expr = new QLineEdit{row};
      expr->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
      expr->setPlaceholderText(tr("Device.read(\"osc:/level\") > 0.5"));
      expr->setToolTip(tr("A JavaScript expression, run in the console's engine."));
      expr->setText(f.script);
      connect(expr, &QLineEdit::editingFinished, this, [this, i, expr] {
        if(m_feedbacks[i].script == expr->text())
          return;
        m_feedbacks[i].script = expr->text();
        commit();
      });
      grid->addWidget(expr, 1, 0, 1, 3);
      break;
    }
  }

  if(f.needsAddress())
  {
    auto cond = new QComboBox{row};
    for(int c = 0; c < Feedback::Script; c++)
      cond->addItem(tr(condition_labels[c]));
    cond->setCurrentIndex(f.condition);
    grid->addWidget(cond, 2, 0);

    auto value = new QLineEdit{row};
    value->setPlaceholderText(tr("value"));
    value->setText(valueToString(f.value));
    value->setVisible(f.needsValue());
    grid->addWidget(value, 2, 1, 1, 2);

    connect(cond, &QComboBox::currentIndexChanged, this, [this, i, value](int c) {
      m_feedbacks[i].condition = Feedback::Condition(c);
      value->setVisible(m_feedbacks[i].needsValue());
      commit();
    });
    connect(value, &QLineEdit::editingFinished, this, [this, i, value] {
      auto v = valueFromString(value->text());
      if(v == m_feedbacks[i].value)
        return;
      m_feedbacks[i].value = v;
      commit();
    });
  }

  connect(source, &QComboBox::currentIndexChanged, this, [this, i](int s) {
    auto& fb = m_feedbacks[i];
    fb.deckInput = s == 1;
    if(s == 2)
      fb.condition = Feedback::Script;
    else if(fb.condition == Feedback::Script)
      fb.condition = Feedback::IsTrue;
    if(s != 0)
      fb.address = s == 1 ? m_defaultName : QString{};
    QTimer::singleShot(0, this, [this] {
      rebuild();
      commit();
    });
  });

  auto style = new OverrideEditor{row};
  style->setOverride(f.style);
  connect(style, &OverrideEditor::changed, this, [this, i, style] {
    m_feedbacks[i].style = style->value();
    commit();
  });
  grid->addWidget(new QLabel{tr("the key looks like:"), row}, 3, 0, 1, 3);
  grid->addWidget(style, 4, 0, 1, 3);
  grid->setColumnStretch(1, 1);
  protectFromWheel(row);
  return row;
}

OverrideEditor::OverrideEditor(QWidget* parent)
    : QWidget{parent}
{
  auto lay = new QGridLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setHorizontalSpacing(6);
  lay->setVerticalSpacing(2);

  m_useBackground = new QCheckBox{tr("Background"), this};
  m_background = new ColorButton{this};
  m_useTextColor = new QCheckBox{tr("Text colour"), this};
  m_textColor = new ColorButton{this};
  m_textColor->setColor(Qt::white);
  m_useText = new QCheckBox{tr("Text"), this};
  m_text = new QLineEdit{this};
  m_text->setPlaceholderText(tr("unchanged"));
  setToolTip(tr("Tick what changes; the rest stays as the key's own look."));

  lay->addWidget(m_useBackground, 0, 0);
  lay->addWidget(m_background, 0, 1);
  lay->addWidget(m_useTextColor, 0, 2);
  lay->addWidget(m_textColor, 0, 3);
  lay->addWidget(m_useText, 1, 0);
  lay->addWidget(m_text, 1, 1, 1, 4);
  lay->setColumnStretch(4, 1);

  auto emitChanged = [this] { changed(); };
  connect(m_useBackground, &QCheckBox::toggled, this, emitChanged);
  connect(m_useTextColor, &QCheckBox::toggled, this, emitChanged);
  connect(m_useText, &QCheckBox::toggled, this, emitChanged);
  connect(m_background, &ColorButton::colorChanged, this, [this] {
    m_useBackground->setChecked(true);
    changed();
  });
  connect(m_textColor, &ColorButton::colorChanged, this, [this] {
    m_useTextColor->setChecked(true);
    changed();
  });
  connect(m_text, &QLineEdit::editingFinished, this, [this] {
    m_useText->setChecked(true);
    changed();
  });
}

void OverrideEditor::setOverride(const StyleOverride& o)
{
  QSignalBlocker b1{m_useBackground}, b2{m_useTextColor}, b3{m_useText},
      b4{m_background}, b5{m_textColor}, b6{m_text};
  m_useBackground->setChecked(bool(o.background));
  if(o.background)
    m_background->setColor(*o.background);
  m_useTextColor->setChecked(bool(o.textColor));
  if(o.textColor)
    m_textColor->setColor(*o.textColor);
  m_useText->setChecked(bool(o.text));
  m_text->setText(o.text.value_or(QString{}));
}

StyleOverride OverrideEditor::value() const
{
  StyleOverride o;
  if(m_useBackground->isChecked())
    o.background = m_background->color();
  if(m_useTextColor->isChecked())
    o.textColor = m_textColor->color();
  if(m_useText->isChecked())
    o.text = m_text->text();
  return o;
}
}
