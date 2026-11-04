#include "Script.hpp"

#include <JS/ApplicationPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <StreamDeck/Engine.hpp>

#include <QJSEngine>

#include <wobjectimpl.h>
W_OBJECT_IMPL(StreamDeck::DeckObject)

namespace StreamDeck
{
DeckObject::DeckObject(Engine& e, QObject* parent)
    : QObject{parent}
    , m_engine{e}
{
}

int DeckObject::page() const
{
  return m_engine.page() + 1;
}

QString DeckObject::pageName() const
{
  const auto& pages = m_engine.layout().pages;
  const int p = m_engine.page();
  return p < int(pages.size()) ? pages[p].name : QString{};
}

int DeckObject::pageCount() const
{
  return int(m_engine.layout().pages.size());
}

void DeckObject::setPage(int page)
{
  if(!readOnly)
    m_engine.setPage(page - 1);
}

void DeckObject::nextPage()
{
  if(!readOnly)
    m_engine.nextPage();
}

void DeckObject::previousPage()
{
  if(!readOnly)
    m_engine.previousPage();
}

int DeckObject::brightness() const
{
  return m_engine.brightness();
}

void DeckObject::setBrightness(int b)
{
  if(!readOnly)
    m_engine.setBrightness(b);
}

Scripts::Scripts(Engine& e)
    : m_owner{e}
{
}

Scripts::~Scripts()
{
  m_compiled.clear();
  m_deckValue = {};
  delete m_deck;
}

namespace
{
QString templateLiteral(const QString& text)
{
  QString res;
  res.reserve(text.size() + 8);
  res += QChar{'`'};
  int depth = 0;
  QChar quote;
  for(qsizetype i = 0; i < text.size(); i++)
  {
    QChar c = text[i];
    if(depth == 0)
    {
      if(c == QChar{'$'} && i + 1 < text.size() && text[i + 1] == QChar{'{'})
      {
        res += QStringLiteral("${");
        depth = 1;
        i++;
      }
      else if(c == QChar{'`'} || c == QChar{'\\'})
      {
        res += QChar{'\\'};
        res += c;
      }
      else
      {
        res += c;
      }
    }
    else
    {
      if(quote != QChar{})
      {
        if(c == QChar{'\\'} && i + 1 < text.size())
        {
          res += c;
          c = text[++i];
        }
        else if(c == quote)
          quote = QChar{};
      }
      else if(c == QChar{'"'} || c == QChar{'\''} || c == QChar{'`'})
        quote = c;
      else if(c == QChar{'{'})
        depth++;
      else if(c == QChar{'}'})
        depth--;
      res += c;
    }
  }
  res += QChar{'`'};
  return res;
}
}

bool Scripts::isTemplate(const QString& text) noexcept
{
  return text.contains(QStringLiteral("${"));
}

namespace
{
struct ReadOnly
{
  DeckObject* deck;
  explicit ReadOnly(DeckObject* d)
      : deck{d}
  {
    if(deck)
      deck->readOnly = true;
  }
  ~ReadOnly()
  {
    if(deck)
      deck->readOnly = false;
  }
};
}

bool Scripts::ensureEngine()
{
  if(m_engine)
    return true;
  if(m_deck)
    return false;

  auto& ctx = score::GUIAppContext();
  auto plug = ctx.findGuiApplicationPlugin<JS::ApplicationPlugin>();
  if(!plug)
    return false;
  m_engine = &plug->m_consoleEngine;
  m_deck = new DeckObject{m_owner, nullptr};
  QJSEngine::setObjectOwnership(m_deck, QJSEngine::CppOwnership);
  m_deckValue = m_engine->newQObject(m_deck);
  return true;
}

void Scripts::clear()
{
  m_compiled.clear();
}

QJSValue Scripts::compile(const QString& source)
{
  auto it = m_compiled.find(source);
  if(it != m_compiled.end())
    return *it;
  auto fn = m_engine->evaluate(source, QStringLiteral("Stream Deck"));
  if(m_engine->hasError())
    fn = m_engine->catchError();
  if(m_compiled.size() > 1024)
    m_compiled.clear();
  m_compiled.insert(source, fn);
  return fn;
}

QJSValue Scripts::call(const QString& source, int key, QString& error)
{
  error.clear();
  if(!ensureEngine())
  {
    error = QObject::tr("JavaScript is not available");
    return {};
  }

  auto fn = compile(
      QStringLiteral("(function(deck, key) { try { return { v: (") + source
      + QStringLiteral(")(deck, key) }; } catch(e) { return { e: String(e) }; } })"));
  if(!fn.isCallable())
  {
    error = fn.toString();
    return {};
  }

  auto res = fn.call({m_deckValue, QJSValue{key + 1}});
  if(m_engine->hasError())
  {
    error = m_engine->catchError().toString();
    return {};
  }
  if(res.isError())
  {
    error = res.toString();
    return {};
  }
  if(res.hasOwnProperty(QStringLiteral("e")))
  {
    error = res.property(QStringLiteral("e")).toString();
    return {};
  }
  return res.property(QStringLiteral("v"));
}

QString Scripts::text(const QString& text, int key, QString& error)
{
  if(!isTemplate(text))
  {
    error.clear();
    return text;
  }
  ensureEngine();
  ReadOnly ro{m_deck};
  auto res = call(
      QStringLiteral("(function(deck, key) { return ") + templateLiteral(text)
          + QStringLiteral("; })"),
      key, error);
  return error.isEmpty() ? res.toString() : QStringLiteral("⚠");
}

bool Scripts::condition(const QString& expr, int key, QString& error)
{
  if(expr.trimmed().isEmpty())
  {
    error.clear();
    return false;
  }
  ensureEngine();
  ReadOnly ro{m_deck};
  auto res = call(
      QStringLiteral("(function(deck, key) { return (\n") + expr
          + QStringLiteral("\n); })"),
      key, error);
  return error.isEmpty() && res.toBool();
}

void Scripts::run(const QString& code, int key, QString& error)
{
  if(code.trimmed().isEmpty())
  {
    error.clear();
    return;
  }
  call(
      QStringLiteral("(function(deck, key) {\n") + code + QStringLiteral("\n})"), key,
      error);
}
}
