#include "ButtonEditor.hpp"

#include <State/Address.hpp>

#include <Process/Dataflow/AddressAccessorEditWidget.hpp>

#include <StreamDeck/Editor/ActionList.hpp>
#include <StreamDeck/Editor/ColorButton.hpp>
#include <StreamDeck/Editor/WheelGuard.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::TextField)
W_OBJECT_IMPL(StreamDeck::ButtonEditor)

namespace StreamDeck
{
namespace
{
QScrollArea* scrolled(QWidget* w)
{
  auto s = new QScrollArea;
  s->setWidgetResizable(true);
  s->setFrameShape(QFrame::NoFrame);
  s->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  s->setWidget(w);
  return s;
}

QLabel* section(const QString& text, QWidget* parent)
{
  auto l = new QLabel{text, parent};
  auto f = l->font();
  f.setBold(true);
  l->setFont(f);
  l->setContentsMargins(0, 8, 0, 0);
  return l;
}

TextField* textField(QWidget* parent)
{
  auto t = new TextField{parent};
  t->setTabChangesFocus(true);
  t->setFixedHeight(t->fontMetrics().lineSpacing() * 3 + 10);
  t->setPlaceholderText(QObject::tr("Text"));
  t->setToolTip(QObject::tr("${...}: JavaScript"));
  return t;
}

QLabel* errorLabel(QWidget* parent)
{
  auto l = new QLabel{parent};
  l->setWordWrap(true);
  auto pal = l->palette();
  pal.setColor(QPalette::WindowText, QColor{0xff, 0x6b, 0x5b});
  l->setPalette(pal);
  l->setTextInteractionFlags(Qt::TextSelectableByMouse);
  l->setContentsMargins(6, 0, 6, 0);
  l->hide();
  return l;
}

void setText(TextField* t, const QString& s)
{
  if(t->toPlainText() != s)
  {
    QSignalBlocker b{t};
    t->setPlainText(s);
  }
}
}

void TextField::focusOutEvent(QFocusEvent* e)
{
  QPlainTextEdit::focusOutEvent(e);
  editingFinished();
}

ButtonEditor::ButtonEditor(const score::DocumentContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_ctx{ctx}
{
  setMinimumWidth(320);
  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);

  m_stack = new QStackedWidget{this};
  m_message = new QLabel;
  m_message->setAlignment(Qt::AlignCenter);
  m_message->setWordWrap(true);
  m_message->setMargin(12);
  m_stack->addWidget(m_message);
  m_stack->addWidget(makeButtonForm());
  m_stack->addWidget(scrolled(makeDialForm()));
  lay->addWidget(m_stack);
}

QWidget* ButtonEditor::makeButtonForm()
{
  auto root = new QWidget;
  auto rootLay = new QVBoxLayout{root};
  rootLay->setContentsMargins(0, 0, 0, 0);
  m_buttonTitle = new QLabel{root};
  auto tf = m_buttonTitle->font();
  tf.setBold(true);
  m_buttonTitle->setFont(tf);
  m_buttonTitle->setContentsMargins(6, 4, 6, 0);
  rootLay->addWidget(m_buttonTitle);
  m_buttonError = errorLabel(root);
  rootLay->addWidget(m_buttonError);
  auto tabs = new QTabWidget{root};
  tabs->setDocumentMode(true);
  rootLay->addWidget(tabs, 1);

  auto page = [tabs](const QString& name) {
    auto w = new QWidget;
    auto lay = new QVBoxLayout{w};
    lay->setContentsMargins(6, 4, 6, 4);
    tabs->addTab(scrolled(w), name);
    return std::make_pair(w, lay);
  };

  auto [w, lay] = page(tr("Look"));
  m_text = textField(w);
  lay->addWidget(m_text);

  auto form = new QFormLayout;
  m_background = new ColorButton{w};
  m_textColor = new ColorButton{w};
  auto colors = new QHBoxLayout;
  colors->addWidget(new QLabel{tr("Background"), w});
  colors->addWidget(m_background);
  colors->addSpacing(8);
  colors->addWidget(new QLabel{tr("Text colour"), w});
  colors->addWidget(m_textColor);
  colors->addStretch(1);
  form->addRow(colors);

  m_textSize = new QSpinBox{w};
  m_textSize->setRange(0, 200);
  m_textSize->setSpecialValueText(tr("Fit"));
  m_textSize->setSuffix(tr(" px"));
  m_textSize->setKeyboardTracking(false);
  m_align = new QComboBox{w};
  m_align->addItems({tr("Middle"), tr("Top"), tr("Bottom")});
  auto sizeRow = new QHBoxLayout;
  sizeRow->addWidget(m_textSize);
  sizeRow->addWidget(m_align);
  form->addRow(tr("Text size"), sizeRow);

  m_image = new QLineEdit{w};
  m_image->setPlaceholderText(tr("Image file"));
  auto browse = new QPushButton{tr("Choose…"), w};
  auto clearImage = new QPushButton{tr("Remove"), w};
  auto imageRow = new QHBoxLayout;
  imageRow->addWidget(m_image, 1);
  imageRow->addWidget(browse);
  imageRow->addWidget(clearImage);
  form->addRow(tr("Image"), imageRow);
  lay->addLayout(form);

  m_activeLabel = section({}, w);
  lay->addWidget(m_activeLabel);
  m_active = new OverrideEditor{w};
  lay->addWidget(m_active);
  lay->addStretch(1);

  auto [aw, alay] = page(tr("Actions"));
  auto bform = new QFormLayout;
  m_mode = new QComboBox{aw};
  m_mode->addItems({tr("Momentary"), tr("Toggle")});
  bform->addRow(tr("Mode"), m_mode);
  m_hold = new QSpinBox{aw};
  m_hold->setRange(0, 10000);
  m_hold->setSingleStep(100);
  m_hold->setSuffix(tr(" ms"));
  m_hold->setSpecialValueText(tr("No"));
  m_hold->setKeyboardTracking(false);
  bform->addRow(tr("Hold for"), m_hold);
  alay->addLayout(bform);

  m_pressLabel = section({}, aw);
  alay->addWidget(m_pressLabel);
  m_press = new ActionList{m_ctx, aw};
  alay->addWidget(m_press);

  m_releaseLabel = section({}, aw);
  alay->addWidget(m_releaseLabel);
  m_release = new ActionList{m_ctx, aw};
  alay->addWidget(m_release);
  alay->addStretch(1);

  auto [fw, flay] = page(tr("Feedbacks"));
  m_feedbacks = new FeedbackList{m_ctx, fw};
  flay->addWidget(m_feedbacks);
  flay->addStretch(1);

  auto changed = [this] { buttonChanged(); };
  connect(m_text, &TextField::editingFinished, this, changed);
  connect(m_textColor, &ColorButton::colorChanged, this, changed);
  connect(m_background, &ColorButton::colorChanged, this, changed);
  connect(m_textSize, &QSpinBox::valueChanged, this, changed);
  connect(m_align, &QComboBox::currentIndexChanged, this, changed);
  connect(m_image, &QLineEdit::editingFinished, this, changed);
  connect(browse, &QPushButton::clicked, this, [this] {
    auto f = QFileDialog::getOpenFileName(
        this, tr("Key image"), m_image->text(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg *.webp)"));
    if(!f.isEmpty())
    {
      m_image->setText(f);
      buttonChanged();
    }
  });
  connect(clearImage, &QPushButton::clicked, this, [this] {
    m_image->clear();
    buttonChanged();
  });
  connect(m_mode, &QComboBox::currentIndexChanged, this, [this] {
    updateModeLabels();
    buttonChanged();
  });
  connect(m_hold, &QSpinBox::valueChanged, this, changed);
  connect(m_active, &OverrideEditor::changed, this, changed);
  connect(m_press, &ActionList::changed, this, changed);
  connect(m_release, &ActionList::changed, this, changed);
  connect(m_feedbacks, &FeedbackList::changed, this, changed);
  protectFromWheel(root);
  return root;
}

QWidget* ButtonEditor::makeDialForm()
{
  auto w = new QWidget;
  auto lay = new QVBoxLayout{w};
  lay->setContentsMargins(6, 4, 6, 4);

  m_dialTitle = new QLabel{w};
  auto tf = m_dialTitle->font();
  tf.setBold(true);
  m_dialTitle->setFont(tf);
  lay->addWidget(m_dialTitle);
  m_dialError = errorLabel(w);
  lay->addWidget(m_dialError);

  lay->addWidget(section(tr("Address"), w));
  m_dialAddress = new Process::AddressAccessorEditWidget{m_ctx, w};
  lay->addWidget(m_dialAddress);
  m_dialStep = new QDoubleSpinBox{w};
  m_dialStep->setRange(-1e6, 1e6);
  m_dialStep->setDecimals(4);
  m_dialStep->setPrefix(tr("Step "));
  m_dialStep->setKeyboardTracking(false);
  lay->addWidget(m_dialStep);

  lay->addWidget(section(tr("Strip"), w));
  m_dialText = textField(w);
  m_dialText->setPlaceholderText(tr("Address and value"));
  lay->addWidget(m_dialText);
  auto colors = new QHBoxLayout;
  m_dialBackground = new ColorButton{w};
  m_dialTextColor = new ColorButton{w};
  colors->addWidget(new QLabel{tr("Background"), w});
  colors->addWidget(m_dialBackground);
  colors->addSpacing(8);
  colors->addWidget(new QLabel{tr("Text colour"), w});
  colors->addWidget(m_dialTextColor);
  colors->addStretch(1);
  lay->addLayout(colors);

  lay->addWidget(section(tr("On press"), w));
  m_dialPress = new ActionList{m_ctx, w};
  lay->addWidget(m_dialPress);
  lay->addStretch(1);

  auto changed = [this] { dialChanged(); };
  connect(
      m_dialAddress, &Process::AddressAccessorEditWidget::addressChanged, this, [this] {
    m_dialAddressText = m_dialAddress->addressString();
    dialChanged();
  });
  connect(m_dialStep, &QDoubleSpinBox::valueChanged, this, changed);
  connect(m_dialText, &TextField::editingFinished, this, changed);
  connect(m_dialBackground, &ColorButton::colorChanged, this, changed);
  connect(m_dialTextColor, &ColorButton::colorChanged, this, changed);
  connect(m_dialPress, &ActionList::changed, this, changed);
  protectFromWheel(w);
  return w;
}

void ButtonEditor::updateModeLabels()
{
  const bool toggle = m_mode->currentIndex() == Button::Toggle;
  m_activeLabel->setText(toggle ? tr("While on") : tr("While pressed"));
  m_pressLabel->setText(toggle ? tr("On") : tr("On press"));
  m_releaseLabel->setText(toggle ? tr("Off") : tr("On release"));
}

void ButtonEditor::setScriptError(const QString& error)
{
  for(auto l : {m_buttonError, m_dialError})
  {
    if(l->text() != error)
      l->setText(error);
    l->setVisible(!error.isEmpty());
  }
}

void ButtonEditor::showMessage(const QString& text)
{
  m_message->setText(text);
  m_stack->setCurrentIndex(0);
}

void ButtonEditor::showButton(
    const QString& title, const Button& b, QStringList pages, QString defaultName)
{
  m_buttonTitle->setText(title);
  setText(m_text, b.style.text);
  {
    QSignalBlocker b1{m_textColor}, b2{m_background}, b3{m_textSize}, b4{m_align},
        b5{m_image}, b6{m_mode}, b7{m_hold}, b8{m_active};
    m_textColor->setColor(b.style.textColor);
    m_background->setColor(b.style.background);
    m_textSize->setValue(b.style.textSize);
    m_align->setCurrentIndex(int(b.style.align));
    if(m_image->text() != b.style.image)
      m_image->setText(b.style.image);
    m_mode->setCurrentIndex(b.mode);
    m_hold->setValue(b.hold);
    m_active->setOverride(b.active);
  }
  {
    QSignalBlocker b1{m_press}, b2{m_release}, b3{m_feedbacks};
    m_press->setActions(b.press, pages, defaultName);
    m_release->setActions(b.release, pages, defaultName + QStringLiteral("/release"));
    m_feedbacks->setFeedbacks(b.feedbacks, defaultName);
  }
  updateModeLabels();
  m_stack->setCurrentIndex(1);
}

void ButtonEditor::showDial(
    const QString& title, const Dial& d, QStringList pages, QString defaultName)
{
  m_dialTitle->setText(title);
  {
    QSignalBlocker b1{m_dialAddress}, b2{m_dialStep}, b3{m_dialBackground},
        b4{m_dialTextColor}, b5{m_dialPress};
    if(d.address != m_dialAddressText)
    {
      m_dialAddressText = d.address;
      if(auto a = State::parseAddressAccessor(d.address))
        m_dialAddress->setAddress(*a);
      else
        m_dialAddress->setAddress({});
    }
    m_dialStep->setValue(d.step);
    m_dialBackground->setColor(d.style.background);
    m_dialTextColor->setColor(d.style.textColor);
    m_dialPress->setActions(d.press, pages, defaultName);
  }
  setText(m_dialText, d.style.text);
  m_stack->setCurrentIndex(2);
}

Button ButtonEditor::button() const
{
  Button b;
  b.style.text = m_text->toPlainText();
  b.style.textColor = m_textColor->color();
  b.style.background = m_background->color();
  b.style.textSize = m_textSize->value();
  b.style.align = Align(m_align->currentIndex());
  b.style.image = m_image->text().trimmed();
  b.mode = Button::Mode(m_mode->currentIndex());
  b.hold = m_hold->value();
  b.active = m_active->value();
  b.press = m_press->actions();
  b.release = m_release->actions();
  b.feedbacks = m_feedbacks->feedbacks();
  return b;
}

Dial ButtonEditor::dial() const
{
  Dial d;
  d.address = m_dialAddressText;
  d.step = m_dialStep->value();
  d.style.text = m_dialText->toPlainText();
  d.style.background = m_dialBackground->color();
  d.style.textColor = m_dialTextColor->color();
  d.press = m_dialPress->actions();
  return d;
}
}
