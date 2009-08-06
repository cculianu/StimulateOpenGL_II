#ifndef FrameVariables_H
#define FrameVariables_H
#include <QStringList>
#include <QFile>
#include <QString>
#include <QVector>

#include "Util.h"

class QTextStream;

class FrameVariables
{
public:
	FrameVariables(const QString & outfile, const QStringList & varnames = QStringList());
	~FrameVariables();
	
	void finalize(); 

	const QStringList & variableNames() const { return var_names; }
	unsigned nFields() const { return n_fields; }
	/// a count of the number of frame vars -- aka the number of times push() has been called.
	unsigned count() const { return cnt; } 
	void setVariableNames(const QStringList & fields);

	void push(double varval0...); ///< all vars must be doubles, and they must be the same number of parameters as the variableNames() list length!

	/// read back the contents of the file to a vector and return it
	static bool readAllFromFile(const QString & filename, QVector<double> & out, int * nrows_out = 0, int * ncols_out = 0, bool matlab = true);
	/// read the last plugin that ran from file
	static bool readAllFromLast(QVector<double> & out, int * nrows_out = 0, int * ncols_out = 0,bool matlab = true) { return readAllFromFile(lastFileName, out, nrows_out, ncols_out, matlab); }
	/// reads the header from the last file written and returns a list of strings...
	static QStringList readHeaderFromLast();
	
	bool readAllFromFile(QVector<double> & out, int * nrows_out = 0, int * ncols_out = 0, bool matlab = true) const;

	QString fileName() const { return fname; }

	static QString makeFileName(const QString & prefix);

	bool readInput(const QString & fileName);
	QVector<double> readNext() { return inp.getNextRow(); }
	void readReset() { inp.curr_row = 0; }

private:
	static QStringList FrameVariables::splitHeader(const QString & ln);
	static QString lastFileName;
	unsigned n_fields, cnt;
	QString fname;
	mutable QFile f;
	mutable QTextStream ts;
	QStringList var_names;

	// lastread stuff
	struct Input {
		Input() : nrows(0), ncols(0), curr_row(0) {}
		QVector<double> allVars;
		int nrows, ncols;
		int curr_row;
		QVector<double> getNextRow();
	} inp;
};
#endif
