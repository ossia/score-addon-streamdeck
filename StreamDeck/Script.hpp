#pragma once
#include <QHash>
#include <QJSValue>
#include <QObject>
#include <QPointer>

#include <verdigris>

class QJSEngine;
namespace StreamDeck
{
class Engine;

class DeckObject final : public QObject
{
  W_OBJECT(DeckObject)
public:
  DeckObject(Engine& e, QObject* parent);

  //! From 1.
  int page() const;
  W_INVOKABLE(page)
  QString pageName() const;
  W_INVOKABLE(pageName)
  int pageCount() const;
  W_INVOKABLE(pageCount)

  void setPage(int page);
  W_INVOKABLE(setPage)
  void nextPage();
  W_INVOKABLE(nextPage)
  void previousPage();
  W_INVOKABLE(previousPage)

  int brightness() const;
  W_INVOKABLE(brightness)
  void setBrightness(int b);
  W_INVOKABLE(setBrightness)

  //! While text and conditions are evaluated: they must not change the deck.
  bool readOnly{};

private:
  Engine& m_engine;
};

class Scripts
{
public:
  explicit Scripts(Engine& e);
  ~Scripts();

  static bool isTemplate(const QString& text) noexcept;

  QString text(const QString& text, int key, QString& error);
  bool condition(const QString& expr, int key, QString& error);
  void run(const QString& code, int key, QString& error);

  void clear();

private:
  QJSValue call(const QString& source, int key, QString& error);
  QJSValue compile(const QString& source);
  bool ensureEngine();

  Engine& m_owner;
  QPointer<QJSEngine> m_engine;
  DeckObject* m_deck{};
  QJSValue m_deckValue;
  QHash<QString, QJSValue> m_compiled;
};
}
