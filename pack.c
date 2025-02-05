/*
 * Routines to deal with the pack
 *
 * @(#)pack.c	4.40 (Berkeley) 02/05/99
 *
 * Rogue: Exploring the Dungeons of Doom
 * Copyright (C) 1980-1983, 1985, 1999 Michael Toy, Ken Arnold and Glenn Wichman
 * All rights reserved.
 *
 * See the file LICENSE.TXT for full copyright and licensing information.
 */

#include <string.h>
#include <curses.h>
#include <ctype.h>
#include "rogue.h"

/*
 * add_pack:
 *	Pick up an object and add it to the pack.  If the argument is
 *	non-null use it as the linked_list pointer instead of gettting
 *	it off the ground.
 */

void add_pack(THING *obj, bool silent) {
  THING *op, *lp;
  bool from_floor;

  from_floor = FALSE;
  if (obj == NULL) {
    if ((obj = find_obj(hero.y, hero.x)) == NULL) return;
    from_floor = TRUE;
  }

  if (from_floor) {
    /*
     * Check for and deal with scare monster scrolls
     */
    if (obj->o_type == SCROLL && obj->o_which == S_SCARE)
      if (obj->o_flags & ISFOUND) {
        detach(&lvl_obj, obj);
        mvaddch(hero.y, hero.x, floor_ch());
        chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
        discard(obj);
        msg("the scroll turns to dust as you pick it up");
        return;
      }
  }

  if (pack == NULL) {
    if (!pack_room(from_floor, obj, TRUE, silent)) return;
    pack = obj;
    inpack++;
  } else {
    lp = NULL;
    for (op = pack; op != NULL; op = next(op)) {
      if (op->o_type != obj->o_type)
        lp = op;
      else {
        while (op->o_type == obj->o_type && op->o_which != obj->o_which) {
          lp = op;
          if (next(op) == NULL)
            break;
          else
            op = next(op);
        }
        if (op->o_type == obj->o_type && op->o_which == obj->o_which) {
          if (ISMULT(op->o_type)) {
            if (!pack_room(from_floor, obj, FALSE, silent)) return;
            op->o_count++;
          dump_it:
            discard(obj);
            obj = op;
            lp = NULL;
            goto out;
          } else if (obj->o_group) {
            lp = op;
            while (op->o_type == obj->o_type && op->o_which == obj->o_which &&
                   op->o_group != obj->o_group) {
              lp = op;
              if (next(op) == NULL)
                break;
              else
                op = next(op);
            }
            if (op->o_type == obj->o_type && op->o_which == obj->o_which &&
                op->o_group == obj->o_group) {
              op->o_count += obj->o_count;
              inpack--;
              if (!pack_room(from_floor, obj, FALSE, silent)) return;
              goto dump_it;
            }
          } else
            lp = op;
        }
      out:
        break;
      }
    }

    if (lp != NULL) {
      if (!pack_room(from_floor, obj, TRUE, silent)) return;
      next(obj) = next(lp);
      prev(obj) = lp;
      if (next(lp) != NULL) prev(next(lp)) = obj;
      next(lp) = obj;
    }
  }

  obj->o_flags |= ISFOUND;

  if (obj->o_packch < 'a' || obj->o_packch > 'z')
    fatal("new object does not have a slot '%s'", unctrl(obj->o_packch));

  /*
   * If this was the object of something's desire, that monster will
   * get mad and run at the hero.
   */
  for (op = mlist; op != NULL; op = next(op))
    if (op->t_dest == &obj->o_pos) op->t_dest = &hero;

  if (obj->o_type == AMULET) amulet = TRUE;
  /*
   * Notify the user
   */
  if (!silent) {
    if (!terse) addmsg("you now have ");
    msg("%s (%c)", inv_name(obj, !terse), obj->o_packch);
  }
}

/*
 * pack_room:
 *	See if there's room in the pack.  If not, print out an
 *	appropriate message
 */
bool pack_room(bool from_floor, THING *obj, bool need_pack_char, bool silent) {
  char ch = 0;
  if (need_pack_char) ch = pack_char();
  if (++inpack > MAXPACK || (need_pack_char && ch == 0)) {
    if (!silent) {
      if (!terse) addmsg("there's ");
      addmsg("no room");
      if (need_pack_char && ch == 0) addmsg(" for slots");
      if (!terse) addmsg(" in your pack");
      endmsg();
    }
    if (from_floor) move_msg(obj);
    inpack = MAXPACK;
    /* undo state changes before returning */
    if (ch != 0) pack_used[ch - 'a'] = FALSE;
    return FALSE;
  }

  if (need_pack_char) obj->o_packch = ch;

  if (from_floor) {
    detach(&lvl_obj, obj);
    mvaddch(hero.y, hero.x, floor_ch());
    chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
  }

  return TRUE;
}

/*
 * leave_pack:
 *	take an item out of the pack
 *
 * implementation note: the old argument oldobj was
 * very confusing, lead to complex and hard to understand code,
 * and to a bug in the nymph code which steal objects.
 * it's much better to always create a new object and the caller
 * discard it if needed.
 */
THING *leave_pack(THING *obj, bool all) {
  THING *nobj = obj;

  inpack--;
  if (obj->o_count > 1 && !all) {
    last_pick = obj;
    obj->o_count--;
    if (obj->o_group) inpack++;
    nobj = new_item();
    *nobj = *obj;
    next(nobj) = NULL;
    prev(nobj) = NULL;
    nobj->o_count = 1;
  } else {
    last_pick = NULL;
    pack_used[obj->o_packch - 'a'] = FALSE;
    detach(&pack, obj);
  }
  return nobj;
}

/*
 * pack_char:
 *	Return the next unused pack character, zero if they are all taken.
 */
char pack_char() {
  char c;
  for (c = 0; c < MAXPACK; c++) {
    if (pack_used[(int)c] == FALSE) {
      pack_used[(int)c] = TRUE;
      return (c + 'a');
    }
  }
  return (0);
}

/*
 * inventory:
 *	List what is in the pack.  Return TRUE if there is something of
 *	the given type.
 */
bool inventory(THING *list, int type) {
  static char inv_temp[MAXSTR];

  n_objs = 0;
  for (; list != NULL; list = next(list)) {
    if (type && type != list->o_type &&
        !(type == CALLABLE && list->o_type != FOOD && list->o_type != AMULET) &&
        !(type == R_OR_S && (list->o_type == RING || list->o_type == STICK)))
      continue;
    n_objs++;
#ifdef MASTER
    if (!list->o_packch)
      strcpy(inv_temp, "%s");
    else
#endif
      sprintf(inv_temp, "%c) %%s", list->o_packch);
    msg_esc = TRUE;
    if (add_line(inv_temp, inv_name(list, FALSE)) == ESCAPE) {
      msg_esc = FALSE;
      msg("");
      return TRUE;
    }
    msg_esc = FALSE;
  }
  if (n_objs == 0) {
    if (terse)
      msg(type == 0 ? "empty handed" : "nothing appropriate");
    else
      msg(type == 0 ? "you are empty handed"
                    : "you don't have anything appropriate");
    return FALSE;
  }
  end_line();
  return TRUE;
}

/*
 * pick_up:
 *	Add something to characters pack.
 */

void pick_up(char ch) {
  THING *obj;

  if (on(player, ISLEVIT)) return;

  obj = find_obj(hero.y, hero.x);
  if (move_on)
    move_msg(obj);
  else
    switch (ch) {
      case GOLD:
        if (obj == NULL) return;
        money(obj->o_goldval);
        detach(&lvl_obj, obj);
        discard(obj);
        proom->r_goldval = 0;
        break;
      default:
#ifdef MASTER
        msg("ERROR: where did you pick a '%s' up???", unctrl(ch));
#endif
      case ARMOR:
      case POTION:
      case FOOD:
      case WEAPON:
      case SCROLL:
      case AMULET:
      case RING:
      case STICK:
        add_pack((THING *)NULL, FALSE);
        break;
    }
}

/*
 * move_msg:
 *	Print out the message if you are just moving onto an object
 */

void move_msg(THING *obj) {
  if (!terse) addmsg("you ");
  msg("moved onto %s", inv_name(obj, TRUE));
}

/*
 * picky_inven:
 *	Allow player to inventory a single item
 */

void picky_inven() {
  THING *obj;
  char mch;

  if (pack == NULL)
    msg("you aren't carrying anything");
  else if (next(pack) == NULL)
    msg("a) %s", inv_name(pack, FALSE));
  else {
    msg(terse ? "item: " : "which item do you wish to inventory: ");
    mpos = 0;
    if ((mch = readchar()) == ESCAPE) {
      msg("");
      return;
    }
    for (obj = pack; obj != NULL; obj = next(obj))
      if (mch == obj->o_packch) {
        msg("%c) %s", mch, inv_name(obj, FALSE));
        return;
      }
    msg("'%s' not in pack", unctrl(mch));
  }
}

/*
 * get_item:
 *	Pick something out of a pack for a purpose
 */
THING *get_item(char *purpose, int type) {
  THING *obj;
  char ch;

  if (pack == NULL)
    msg("you aren't carrying anything");
  else if (again)
    if (last_pick)
      return last_pick;
    else
      msg("you ran out");
  else {
    for (;;) {
      if (!terse) addmsg("which object do you want to ");
      addmsg(purpose);
      if (terse) addmsg(" what");
      msg("? (* for list): ");
      ch = readchar();
      mpos = 0;
      /*
       * Give the poor player a chance to abort the command
       */
      if (ch == ESCAPE) {
        reset_last();
        after = FALSE;
        msg("");
        return NULL;
      }
      n_objs = 1; /* normal case: person types one char */
      if (ch == '*') {
        mpos = 0;
        if (inventory(pack, type) == 0) {
          after = FALSE;
          return NULL;
        }
        continue;
      }
      for (obj = pack; obj != NULL; obj = next(obj))
        if (obj->o_packch == ch) break;
      if (obj == NULL) {
        msg("'%s' is not a valid item", unctrl(ch));
        continue;
      } else
        return obj;
    }
  }
  return NULL;
}

/*
 * money:
 *	Add or subtract gold from the pack
 */

void money(int value) {
  purse += value;
  mvaddch(hero.y, hero.x, floor_ch());
  chat(hero.y, hero.x) = (proom->r_flags & ISGONE) ? PASSAGE : FLOOR;
  if (value > 0) {
    if (!terse) addmsg("you found ");
    msg("%d gold pieces", value);
  }
}

/*
 * floor_ch:
 *	Return the appropriate floor character for her room
 */
char floor_ch() {
  if (proom->r_flags & ISGONE) return PASSAGE;
  return (show_floor() ? FLOOR : ' ');
}

/*
 * floor_at:
 *	Return the character at hero's position, taking see_floor
 *	into account
 */
char floor_at() {
  char ch;

  ch = chat(hero.y, hero.x);
  if (ch == FLOOR) ch = floor_ch();
  return ch;
}

/*
 * reset_last:
 *	Reset the last command when the current one is aborted
 */

void reset_last() {
  last_comm = l_last_comm;
  last_dir = l_last_dir;
  last_pick = l_last_pick;
}

#ifdef MASTER
void check_inventory(THING *list) {
  int u_pack_char[MAXPACK];
  int objs = 0;
  int pos = 0;
  int i;
  memset(u_pack_char, 0, sizeof(u_pack_char));

  for (; list != NULL; list = next(list), pos++) {
    char packch_str[10];
    char type_str[10];
    strcpy(packch_str, unctrl(list->o_packch));
    strcpy(type_str, unctrl(list->o_type));
    if (list->o_packch == 0) {
      msg("WARNING: pack list #%d: type '%s' item '%s' is not selectable", pos,
          type_str, packch_str);
      continue;
    }
    if (list->o_packch < 'a' || list->o_packch > 'z') {
      msg("ERROR: pack list #%d: bad pack type '%s' item '%s'", pos, type_str,
          packch_str);
    } else {
      if (u_pack_char[list->o_packch - 'a']) {
        msg("ERROR: pack list #%d: type '%s' item '%s' already used", pos,
            type_str, packch_str);
      }
      u_pack_char[list->o_packch - 'a'] = 1;
      if (pack_used[list->o_packch - 'a'] == FALSE) {
        msg("ERROR: pack list #%d: type '%s' item '%s': pack_char not marked "
            "as used",
            pos, type_str, packch_str);
      }
    }
    switch (list->o_type) {
      case GOLD:
      case ARMOR:
      case POTION:
      case FOOD:
      case WEAPON:
      case SCROLL:
      case AMULET:
      case RING:
      case STICK:
        objs++;
        break;
      default:
        msg("ERROR: list pos %d: unknown type '%s' item '%s'", pos, type_str,
            packch_str);
    }
  }
  for (i = 0; i < MAXPACK; i++) {
    if (u_pack_char[i] == FALSE && pack_used[i]) {
      msg("ERROR: unused item '%s' marked as used", unctrl(i + 'a'));
    }
  }
}
#endif
