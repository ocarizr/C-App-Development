#ifndef WORKFORCE_EXPORT_H
#define WORKFORCE_EXPORT_H

#include "workforce_report.h"

#include <stdio.h>

/* Presentation adapters accept a report built by the core. Streams are borrowed.
 * CSV streams must be binary; source text is retained with comma separators,
 * quoted text and CRLF records. Formula-like cells receive an apostrophe. */
WorkforceStatus report_write_text(FILE *output, const PayrollReport *report);
WorkforceStatus report_write_csv(FILE *output, const PayrollReport *report);
/* Creates a new file exclusively: existing files are never overwritten. */
WorkforceStatus report_export_csv(const char *path, const PayrollReport *report);

/* Read-only snapshot adapters. Streams are borrowed and flushed, never closed.
 * They format stored values without repository access or business calculations. */
WorkforceStatus team_write_text(FILE *output, const Team *team);
WorkforceStatus worker_write_contracts(FILE *output, const Worker *worker);
WorkforceStatus worker_write_history(FILE *output, const Worker *worker);

#endif
