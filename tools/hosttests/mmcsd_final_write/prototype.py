# SPDX-License-Identifier: Apache-2.0
"""Host fixture only: test one possible fail-closed contract."""

def replace_one(source, before, after):
    if source.count(before) != 1:
        raise ValueError(f"Unexpected prototype anchor: {before}")
    return source.replace(before, after, 1)


def connected(definitions):
    status, ready, write, close = definitions
    ready = replace_one(
        ready,
        "  /* First, check if the card has been removed. */",
        """  /* Host-only prototype: a readiness check cannot retire an
   * uncertain command or transfer. Retain custody and refuse new I/O. */

  if (priv->write_uncertain)
    {
      return -EIO;
    }

  /* First, check if the card has been removed. */""",
    )
    ready = replace_one(
        ready,
        "if (IS_STATE(r1, MMCSD_R1_STATE_TRAN))",
        "if (IS_STATE(r1, MMCSD_R1_STATE_TRAN) &&\n"
        "          (r1 & MMCSD_R1_READYFORDATA) != 0)",
    )
    ready = replace_one(
        ready,
        "!IS_STATE(r1, MMCSD_R1_STATE_RCV))",
        "!IS_STATE(r1, MMCSD_R1_STATE_RCV) &&\n"
        "               !IS_STATE(r1, MMCSD_R1_STATE_TRAN))",
    )
    write = replace_one(
        write,
        "  DEBUGASSERT(priv != NULL && buffer != NULL);",
        """  DEBUGASSERT(priv != NULL && buffer != NULL);

  /* Refuse before partition switches, setup, or additional CMD24. */

  if (priv->write_uncertain)
    {
      return -EIO;
    }""",
    )
    write = replace_one(
        write,
        '      ferr("ERROR: CMD24 transfer failed: %d\\n", ret);\n      return ret;',
        """      /* No card state or hardware retirement established. */
      priv->write_uncertain = true;
      ferr("ERROR: CMD24 transfer failed: %d\\n", ret);
      return ret;""",
    )
    return status, ready, write, close
