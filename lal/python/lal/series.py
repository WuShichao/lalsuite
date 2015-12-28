#!/usr/bin/env python
#
# Copyright (C) 2008  Kipp Cannon
#               2015  Leo Singer
#
# Adapted from original pylal.series module to return SWIG lal datatypes
# instead of pylal datatypes.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
"""
Code to assist in reading and writing LAL time- and frequency series data
encoded in LIGO Light-Weight XML format.  The format recognized by the code
in this module is the same as generated by the array-related functions in
LAL's XML I/O code.  The format is also very similar to the format used by
the DMT to store time- and frequency-series data in XML files,
"""


from glue.ligolw import ligolw
from glue.ligolw import array as ligolw_array
from glue.ligolw import param as ligolw_param
from glue.ligolw import table as ligolw_table
from glue.ligolw import types as ligolw_types
import lal
import numpy as np


Attributes = ligolw.sax.xmlreader.AttributesImpl


#
# =============================================================================
#
#                                   XML I/O
#
# =============================================================================
#


def _build_series(series, dim_names, comment, delta_name, delta_unit):
    elem = ligolw.LIGO_LW(Attributes({u"Name": unicode(series.__class__.__name__)}))
    if comment is not None:
        elem.appendChild(ligolw.Comment()).pcdata = comment
    # FIXME:  make Time class smart so we don't have to build it by
    # hand
    elem.appendChild(ligolw.Time(Attributes({u"Name": u"epoch", u"Type": u"GPS"}))).pcdata = unicode(series.epoch)
    elem.appendChild(ligolw_param.from_pyvalue(u"f0", series.f0, unit=u"s^-1"))
    delta = getattr(series, delta_name)
    if np.iscomplexobj(series.data.data):
        data = np.row_stack((np.arange(len(series.data.data)) * delta, series.data.data.real, series.data.data.imag))
    else:
        data = np.row_stack((np.arange(len(series.data.data)) * delta, series.data.data))
    a = ligolw_array.from_array(series.name, data, dim_names=dim_names)
    a.Unit = str(series.sampleUnits)
    dim0 = a.getElementsByTagName(ligolw.Dim.tagName)[0]
    dim0.Unit = delta_unit
    dim0.Start = series.f0
    dim0.Scale = delta
    elem.appendChild(a)
    return elem


def _parse_series(elem, creatorfunc, delta_target_unit_string):
    t, = elem.getElementsByTagName(ligolw.Time.tagName)
    a, = elem.getElementsByTagName(ligolw.Array.tagName)
    dims = a.getElementsByTagName(ligolw.Dim.tagName)
    f0 = ligolw_param.get_param(elem, u"f0")

    epoch = lal.LIGOTimeGPS(str(t.pcdata))

    # Target units: inverse seconds
    inverse_seconds_unit = lal.Unit("s^-1")

    delta_target_unit = lal.Unit(delta_target_unit_string)

    # Parse units of f0 field
    f0_unit = lal.Unit(str(f0.Unit))

    # Parse units of deltaF field
    delta_unit = lal.Unit(str(dims[0].Unit))

    # Parse units of data
    sample_unit = lal.Unit(str(a.Unit))

    # Initialize data structure
    series = creatorfunc(
        str(a.Name),
        epoch,
        float(f0.pcdata) * float(f0_unit / inverse_seconds_unit),
        float(dims[0].Scale) * float(delta_unit / delta_target_unit),
        sample_unit,
        len(a.array.T)
    )

    # Assign data
    if np.iscomplexobj(series.data.data):
        series.data.data = a.array[1] + 1j * a.array[2]
    else:
        series.data.data = a.array[1]

    # Done!
    return series


def build_REAL4FrequencySeries(series, comment=None):
    assert isinstance(series, lal.REAL4FrequencySeries)
    return _build_series(series, (u"Frequency", u"Frequency,Real"), comment, 'deltaF', 's^-1')


def parse_REAL4FrequencySeries(elem):
    return _parse_series(elem, lal.CreateREAL4FrequencySeries, "s^-1")


def build_REAL8FrequencySeries(series, comment=None):
    assert isinstance(series, lal.REAL8FrequencySeries)
    return _build_series(series, (u"Frequency", u"Frequency,Real"), comment, 'deltaF', 's^-1')


def parse_REAL8FrequencySeries(elem):
    return _parse_series(elem, lal.CreateREAL8FrequencySeries, "s^-1")


def build_COMPLEX8FrequencySeries(series, comment=None):
    assert isinstance(series, lal.COMPLEX8FrequencySeries)
    return _build_series(series, (u"Frequency", u"Frequency,Real,Imaginary"), comment, 'deltaF', 's^-1')


def parse_COMPLEX8FrequencySeries(elem):
    return _parse_series(elem, lal.CreateCOMPLEX8FrequencySeries, "s^-1")


def build_COMPLEX16FrequencySeries(series, comment=None):
    assert isinstance(series, lal.COMPLEX16FrequencySeries)
    return _build_series(series, (u"Frequency", u"Frequency,Real,Imaginary"), comment, 'deltaF', 's^-1')


def parse_COMPLEX16FrequencySeries(elem):
    return _parse_series(elem, lal.CreateCOMPLEX16FrequencySeries, "s^-1")


def build_REAL4TimeSeries(series, comment=None):
    assert isinstance(series, lal.REAL4TimeSeries)
    return _build_series(series, (u"Time", u"Time,Real"), comment, 'deltaT', 's')


def parse_REAL4TimeSeries(elem):
    return _parse_series(elem, lal.CreateREAL4TimeSeries, "s")


def build_REAL8TimeSeries(series, comment=None):
    assert isinstance(series, lal.REAL8TimeSeries)
    return _build_series(series, (u"Time", u"Time,Real"), comment, 'deltaT', 's')


def parse_REAL8TimeSeries(elem):
    return _parse_series(elem, lal.CreateREAL8TimeSeries, "s")


def build_COMPLEX8TimeSeries(series, comment=None):
    assert isinstance(series, lal.COMPLEX8TimeSeries)
    return _build_series(series, (u"Time", u"Time,Real,Imaginary"), comment, 'deltaT', 's')


def parse_COMPLEX8TimeSeries(elem):
    return _parse_series(elem, lal.CreateCOMPLEX8TimeSeries, "s")


def build_COMPLEX16TimeSeries(series, comment=None):
    assert isinstance(series, lal.COMPLEX16TimeSeries)
    return _build_series(series, (u"Time", u"Time,Real,Imaginary"), comment, 'deltaT', 's')


def parse_COMPLEX16TimeSeries(elem):
    return _parse_series(elem, lal.CreateCOMPLEX16TimeSeries, "s")


#
# =============================================================================
#
#                                 XML PSD I/O
#
# =============================================================================
#


def make_psd_xmldoc(psddict, xmldoc = None):
    """
    Construct an XML document tree representation of a dictionary of
    frequency series objects containing PSDs.  See also
    read_psd_xmldoc() for a function to parse the resulting XML
    documents.

    If xmldoc is None (the default), then a new XML document is created
    and the PSD dictionary added to it.  If xmldoc is not None then the
    PSD dictionary is appended to the children of that element inside a
    new LIGO_LW element.
    """
    if xmldoc is None:
        xmldoc = ligolw.Document()
    lw = xmldoc.appendChild(ligolw.LIGO_LW())
    for instrument, psd in psddict.items():
        fs = lw.appendChild(build_REAL8FrequencySeries(psd))
        if instrument is not None:
            fs.appendChild(ligolw_param.from_pyvalue(u"instrument", instrument))
    return xmldoc


def read_psd_xmldoc(xmldoc):
    """
    Parse a dictionary of PSD frequency series objects from an XML
    document.  See also make_psd_xmldoc() for the construction of XML documents
    from a dictionary of PSDs.  Interprets an empty freuency series for an
    instrument as None.
    """
    out = dict(
        (ligolw_param.get_pyvalue(elem, u"instrument"),
        parse_REAL8FrequencySeries(elem))
        for elem in xmldoc.getElementsByTagName(ligolw.LIGO_LW.tagName)
        if elem.hasAttribute(u"Name")
        and elem.Name == u"REAL8FrequencySeries")
    # Interpret empty frequency series as None
    for k in out:
        if len(out[k].data.data) == 0:
            out[k] = None
    return out


@ligolw_array.use_in
@ligolw_param.use_in
class PSDContentHandler(ligolw.LIGOLWContentHandler):
    """A content handler suitable for reading PSD documents. Use like this:

    >>> from glue.ligolw.utils import load_filename
    >>> xmldoc = load_filename('psd.xml', contenthandler=PSDContentHandler)
    >>> psds = read_psd_xmldoc(xmldoc)
    """
