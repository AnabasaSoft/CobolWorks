/******************************************************************************
 * Proyecto: CobolWorks IDE
 * Autor: AnabasaSoft (https://github.com/anabasasoft)
 * Licencia: GNU General Public License v3.0 (GPL-3.0)
 * * Descripción: IDE moderno para COBOL con análisis de flujo, depuración visual
 * e integración con IA para migración de código.
 * * "Domina el código Legacy con herramientas del futuro."
 *****************************************************************************/

#ifndef COBOLLEXER_H
#define COBOLLEXER_H

#include <Qsci/qscilexercustom.h>

class CobolLexer : public QsciLexerCustom {
    Q_OBJECT
public:
    enum {
        Default = 0,
        Keyword = 1,
        String = 2,
        Comment = 3,
        Number = 4
    };

    CobolLexer(QObject *parent = nullptr);
    ~CobolLexer();

    const char *language() const override;
    QString description(int style) const override;
    void styleText(int start, int end) override;
    QColor defaultColor(int style) const override;
    QFont defaultFont(int style) const override;
    QColor defaultPaper(int style) const override;
    const char *wordCharacters() const override;
};

#endif // COBOLLEXER_H
