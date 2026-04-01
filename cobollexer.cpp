/******************************************************************************
 * Proyecto: CobolWorks IDE
 * Autor: AnabasaSoft (https://github.com/anabasasoft)
 * Licencia: GNU General Public License v3.0 (GPL-3.0)
 * * Descripción: IDE moderno para COBOL con análisis de flujo, depuración visual
 * e integración con IA para migración de código.
 * * "Domina el código Legacy con herramientas del futuro."
 *****************************************************************************/

#include "cobollexer.h"
#include <Qsci/qsciscintilla.h>
#include <QColor>
#include <QFont>
#include <QRegularExpression>

CobolLexer::CobolLexer(QObject *parent) : QsciLexerCustom(parent) {}
CobolLexer::~CobolLexer() {}

const char *CobolLexer::language() const { return "COBOL"; }

QString CobolLexer::description(int style) const {
    switch(style) {
        case Default: return "Default";
        case Keyword: return "Keyword";
        case String: return "String";
        case Comment: return "Comment";
        case Number: return "Number";
    }
    return QString();
}

QColor CobolLexer::defaultColor(int style) const {
    switch(style) {
        case Default: return QColor("#E2F1E7");
        case Keyword: return QColor("#E5C07B"); // Amarillo cálido
        case String:  return QColor("#98C379"); // Verde suave
        case Comment: return QColor("#8A95A5"); // Gris azulado
        case Number:  return QColor("#D19A66"); // Naranja
    }
    return QsciLexerCustom::defaultColor(style);
}

QColor CobolLexer::defaultPaper(int style) const {
    return QColor("#1E3E62"); // Fondo azul oscuro
}

QFont CobolLexer::defaultFont(int style) const {
    return QFont("Courier New", 12);
}

void CobolLexer::styleText(int start, int end) {
    if (!editor()) return;

    // Obtenemos qué líneas han cambiado realmente
    int lineStart, colStart, lineEnd, colEnd;
    editor()->lineIndexFromPosition(start, &lineStart, &colStart);
    editor()->lineIndexFromPosition(end, &lineEnd, &colEnd);

    // Procesamos línea por línea. Esto soluciona dos problemas:
    // 1. Las expresiones regulares no se cortan a mitad de palabra.
    // 2. Corregimos el desfase de bytes provocado por acentos y caracteres especiales.
    for (int i = lineStart; i <= lineEnd; ++i) {
        QString lineText = editor()->text(i);
        int lineBytePos = editor()->positionFromLineIndex(i, 0);

        // 0. Limpiamos la línea entera poniéndola en Default
        startStyling(lineBytePos);
        setStyling(lineText.toUtf8().length(), Default);

        // Función interna para aplicar estilos calculando los bytes exactos
        auto applyStyleLine = [&](const QRegularExpression &rx, int style) {
            QRegularExpressionMatchIterator it = rx.globalMatch(lineText);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                // Conversión estricta de QChar (UTF-16) a Bytes (UTF-8)
                int byteStart = lineText.left(match.capturedStart()).toUtf8().length();
                int byteLen = lineText.mid(match.capturedStart(), match.capturedLength()).toUtf8().length();
                startStyling(lineBytePos + byteStart);
                setStyling(byteLen, style);
            }
        };

        // 1. Números
        applyStyleLine(QRegularExpression("\\b\\d+(\\.\\d+)?\\b"), Number);

        // 2. Palabras Clave
        applyStyleLine(QRegularExpression("\\b(IDENTIFICATION|ENVIRONMENT|DATA|PROCEDURE|DIVISION|SECTION|PROGRAM-ID|WORKING-STORAGE|LINKAGE|FILE|PIC|PICTURE|DISPLAY|ACCEPT|STOP|RUN|PERFORM|COMPUTE|IF|ELSE|END-IF)\\b", QRegularExpression::CaseInsensitiveOption), Keyword);

        // 3. Cadenas ("..." o '...')
        applyStyleLine(QRegularExpression("(\"[^\"]*\"|'[^']*')"), String);

        // 4. Comentarios (Asterisco en col 7 o el tag *>)
        applyStyleLine(QRegularExpression("(^.{6}\\*.*|\\*>.*)"), Comment);
        // --- PLEGADO DE CÓDIGO (FOLDING) ---
        // Usamos los códigos internos de Scintilla directamente por seguridad
        int SCI_GETFOLDLEVEL = 2223;
        int SCI_SETFOLDLEVEL = 2222;
        int SC_FOLDLEVELBASE = 0x400;
        int SC_FOLDLEVELHEADERFLAG = 0x2000;
        int SC_FOLDLEVELNUMBERMASK = 0x0FFF;

        int foldLevel = SC_FOLDLEVELBASE;
        bool esComentario = (lineText.length() >= 7 && lineText[6] == '*');

        // Si es una División, abrimos el bloque principal
        if (!esComentario && lineText.contains(" DIVISION.", Qt::CaseInsensitive)) {
            foldLevel = SC_FOLDLEVELBASE | SC_FOLDLEVELHEADERFLAG;
        }
        // Si es una Sección, abrimos un sub-bloque dentro de la División
        else if (!esComentario && lineText.contains(" SECTION.", Qt::CaseInsensitive)) {
            foldLevel = (SC_FOLDLEVELBASE + 1) | SC_FOLDLEVELHEADERFLAG;
        }
        // Si es una línea normal de código, hereda el nivel de la línea de arriba
        else {
            if (i > 0) {
                int prevLevelRaw = editor()->SendScintilla(SCI_GETFOLDLEVEL, i - 1);
                int prevLevel = prevLevelRaw & SC_FOLDLEVELNUMBERMASK;
                // Si la línea anterior era la cabecera, nosotros estamos "un nivel más adentro"
                foldLevel = (prevLevelRaw & SC_FOLDLEVELHEADERFLAG) ? (prevLevel + 1) : prevLevel;
            }
        }

        // Inyectamos el cálculo matemático en el motor
        editor()->SendScintilla(SCI_SETFOLDLEVEL, i, foldLevel);
    }
}
