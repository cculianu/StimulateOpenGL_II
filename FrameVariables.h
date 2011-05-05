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
	void setVariableDefaults(const QVector<double> & defaults);
	QVector<double> & variableDefaults() { return var_defaults; } ///< reference to the defaults currently in-use so that plugins can modify them
	void setPrecision(unsigned varNum, int precision=6); ///< default precision for fractional part of outputted variables is 6 digits, but for some save vars, more precision is required

	void push(double varval0...); ///< all vars must be doubles, and they must be the same number of parameters as the variableNames() list length!

	/// read back the contents of the file to a vector and return it
	static bool readAllFromFile(const QString & filename, QVector<double> & out, int * nrows_out = 0, int * ncols_out = 0, bool matlab = true);

	
	/// read the last plugin that ran from file
	static bool readAllFromLast(QVector<double> & out, int * nrows_out = 0, int * ncols_out = 0,bool matlab = true);
	
	/// reads the header from the specified filename and returns a list of strings, sans the enclosing quotes
	static QStringList readHeaderFromFile(const QString & fileName);
	
	/// reads the header from the last file written and returns a list of strings, sansa the enclosing quotes
	static QStringList readHeaderFromLast();
	
	bool readAllFromFile(QVector<double> & out, int * nrows_out = 0, int * ncols_out = 0, bool matlab = true) const;

	QString fileName() const { return fname; }

	static QString makeFileName(const QString & prefix);

	bool readInput(const QString & fileName);
	QVector<double> readNext();
	void readReset() { inp.curr_row = 0; inp.headerRow.clear();  needComputeCols = true; inp.allVars.clear(); inp.col_positions.clear(); }
	bool hasInputColumn(const QString & col_name);
	
	/// called by GLWindow when nLoops and looptCt > 0
	void closeAndRemoveOutput();
	
	bool atEnd() const { return inp.curr_row >= inp.nrows; }

private:
	static QStringList splitHeader(const QString & ln);
	bool computeCols(const QString & fileName);
	bool checkComputeCols();
	
	unsigned n_fields, cnt;
	QString fname, fnameInp;
	mutable QFile f;
	mutable QTextStream ts;
	QStringList var_names;
	QVector<double> var_defaults;
	QVector<unsigned> var_precisions; ///< defaults to 6 for all
	int cantOpenComplainCt;
	bool needComputeCols;
	static QStringList lastFileNames;
	static QString lastFileName();
	static void pushLastFileName(const QString & fn);
	
	// lastread stuff
	struct Input {
		Input() : nrows(0), ncols(0), curr_row(0) { col_positions.clear(); }
		QVector<double> allVars;
		QVector<int> col_positions; ///< each element of this vector is an index in the ideal 'defaults' row
		int nrows, ncols;
		int curr_row;
		QVector<double> getNextRow();
		QStringList headerRow;
	} inp;
};
#endif
