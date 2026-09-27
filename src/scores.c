/*
 * scores.c - scores.txt: one line per entry,
 *   pack <TAB> score <TAB> level <TAB> speed <TAB> date <TAB> name
 * Other packs' lines are kept as they are when one pack's table is saved.
 */

#include "scores.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define LINE_MAX_ 1200

static const char *data_dir(void)
{
    static char dir[1024];
    const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)        snprintf(dir, sizeof dir, "%s/rodentrecomp", xdg);
    else if (home && *home) snprintf(dir, sizeof dir, "%s/.local/share/rodentrecomp", home);
    else                    snprintf(dir, sizeof dir, ".");
    return dir;
}

const char *scores_path(void)
{
    static char path[1100];
    snprintf(path, sizeof path, "%s/scores.txt", data_dir());
    return path;
}

/* splits "pack\tscore\t..." into e; false if the line is not an entry */
static bool parse(char *line, char **pack, Score *e)
{
    char *f[6];
    int i;
    line[strcspn(line, "\r\n")] = '\0';
    for (i = 0; i < 6; i++) {
        f[i] = line;
        line += strcspn(line, "\t");
        if (i < 5) {
            if (*line != '\t') return false;
            *line++ = '\0';
        }
    }
    memset(e, 0, sizeof *e);
    *pack = f[0];
    e->score = atol(f[1]);
    e->level = atoi(f[2]);
    e->speed = atoi(f[3]);
    snprintf(e->date, sizeof e->date, "%.10s", f[4]);
    snprintf(e->name, sizeof e->name, "%.*s", NAME_MAX_, f[5]);
    return true;
}

void scores_load(const char *pack, ScoreTable *t)
{
    char line[LINE_MAX_];
    FILE *f = fopen(scores_path(), "r");
    memset(t, 0, sizeof *t);
    if (!f) return;
    while (fgets(line, sizeof line, f) && t->n < SCORE_MAX) {
        char *p;
        Score e;
        if (parse(line, &p, &e) && !strcmp(p, pack)) t->s[t->n++] = e;
    }
    fclose(f);
}

int scores_rank(const ScoreTable *t, long score)
{
    int i;
    if (score <= 0) return -1;
    for (i = 0; i < t->n; i++)
        if (score > t->s[i].score) return i;      /* ties go below the older entry */
    return t->n < SCORE_MAX ? t->n : -1;
}

static bool make_dirs(const char *dir)
{
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", dir);
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(buf, 0755) != 0 && errno != EEXIST) return false;
        *p = '/';
    }
    return mkdir(buf, 0755) == 0 || errno == EEXIST;
}

/* rewrites the file with pack's table replaced by t */
static bool save(const char *pack, const ScoreTable *t)
{
    char tmp[1200], line[LINE_MAX_];
    FILE *in, *out;
    int i;

    if (!make_dirs(data_dir())) return false;
    snprintf(tmp, sizeof tmp, "%s.tmp", scores_path());
    if (!(out = fopen(tmp, "w"))) return false;
    if ((in = fopen(scores_path(), "r"))) {
        while (fgets(line, sizeof line, in)) {
            char copy[LINE_MAX_], *p;
            Score e;
            snprintf(copy, sizeof copy, "%s", line);
            if (parse(copy, &p, &e) && !strcmp(p, pack)) continue;
            fputs(line, out);
        }
        fclose(in);
    }
    for (i = 0; i < t->n; i++)
        fprintf(out, "%s\t%ld\t%d\t%d\t%s\t%s\n", pack, t->s[i].score, t->s[i].level,
                t->s[i].speed, t->s[i].date, t->s[i].name);
    if (fclose(out) != 0 || rename(tmp, scores_path()) != 0) {
        fprintf(stderr, "Cannot save %s\n", scores_path());
        remove(tmp);
        return false;
    }
    return true;
}

int scores_add(const char *pack, const Score *e)
{
    ScoreTable t;
    Score clean = *e;
    int r, i;

    for (char *c = clean.name; *c; c++)       /* keep the file's columns intact */
        if (*c == '\t' || *c == '\n' || *c == '\r') *c = ' ';
    scores_load(pack, &t);
    if ((r = scores_rank(&t, e->score)) < 0) return -1;
    if (t.n < SCORE_MAX) t.n++;
    for (i = t.n - 1; i > r; i--) t.s[i] = t.s[i - 1];
    t.s[r] = clean;
    return save(pack, &t) ? r : -1;
}

bool scores_clear(const char *pack)
{
    ScoreTable t = { .n = 0 };
    return save(pack, &t);
}
