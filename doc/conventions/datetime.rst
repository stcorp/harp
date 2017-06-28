Datetime values
===============

Datetime values are always represented as a number of days or seconds since a reference time. This is also reflected
by the ``unit`` attribute for datetime values (e.g. ``days since 2000-01-01``). The reference time that is mentioned in
these units should always use UTC as timezone (i.e none of the datetime values should reference a local time in HARP).

In addition, HARP does not deal explicitly with leap seconds in its time calculations. Each day is just treated as
having 24 * 60 * 60 = 86400 seconds (the udunits2 library, which HARP uses internally, has the same behaviour).
In practice, datetime values should be chosen such that they end up being accurate with regard to the UTC epoch that
they represent when using the 86400 seconds per day convention (and will introduce an error when calculating time
differences between epochs if there were leap seconds introduced between those epochs). For instance when representing
``2010-01-01T00:00:00`` as an amount of seconds since 2000, then this is best represented with
``315619200 [s since 2000-01-01]`` and not with ``315619202 [s since 2000-01-01]``.
For cases where it is needed to be interoperable with software that can properly deal with leap seconds, the
recommended approach is to use a reference epoch in the unit such that the represented value is not impacted by leap
seconds. This can, for instance, be achieved by using the start of the day as reference epoch (i.e. represent
``2001-02-03T04:05:06`` as ``14706 [s since 2001-02-03]``).
