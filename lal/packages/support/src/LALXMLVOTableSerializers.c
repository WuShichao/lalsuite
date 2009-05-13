/*
 *  Copyright (C) 2009 Oliver Bock
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */

/**
 * \file
 * \ingroup XML
 * \brief Implementation of the VOTable serializers XML API
 */

#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <lal/XLALError.h>
#include <lal/LALXML.h>
#include <lal/LALXMLVOTableCommon.h>
#include <lal/LALXMLVOTableSerializers.h>


#define INT4STR_MAXLEN          15
#define REAL8STR_MAXLEN         25
#define XPATHSTR_MAXLEN         150


/**
 * \brief Serializes a \c LIGOTimeGPS structure into a VOTable XML %node
 *
 * This function takes a \c LIGOTimeGPS structure and serializes it into a VOTable
 * \c RESOURCE %node identified by the given name. The returned \c xmlNode can then be
 * embedded into an existing %node hierarchy or turned into a full VOTable document.
 *
 * \param ltg [in] Pointer to the \c LIGOTimeGPS structure to be serialized
 * \param name [in] Unique identifier of this particular \c LIGOTimeGPS structure instance
 *
 * \return A pointer to a \c xmlNode that holds the VOTable fragment that represents
 * the \c LIGOTimeGPS structure.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * fragment isn't needed anymore) using \c xmlFreeNode. Alternatively, \c xmlFreeDoc
 * can be used later on when the returned fragment has been embedded in a XML document.
 *
 * \sa XLALCreateVOTableParamNode
 * \sa XLALCreateVOTableResourceNode
 * \sa XLALCreateVOTableXMLFromTree
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
xmlNodePtr XLALLIGOTimeGPS2VOTableNode(const LIGOTimeGPS *const ltg, const char *name)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALLIGOTimeGPS2VOTableNode";
    xmlNodePtr xmlResourceNode = NULL;
    xmlNodePtr xmlResourceParamNodes[2] = {NULL};

    CHAR gpsSecondsBuffer[INT4STR_MAXLEN] = {0};
    CHAR gpsNanoSecondsBuffer[INT4STR_MAXLEN] = {0};

    /* make sure that the shared library is the same as the
     * library version the code was compiled against */
    LIBXML_TEST_VERSION

    /* check and prepare input parameters */
    if(!ltg || LALSnprintf(gpsSecondsBuffer, INT4STR_MAXLEN, "%i", ltg->gpsSeconds) < 0) {
        XLALPrintError("Invalid input parameter: LIGOTimeGPS->gpsSeconds\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!ltg || LALSnprintf(gpsNanoSecondsBuffer, INT4STR_MAXLEN, "%i", ltg->gpsNanoSeconds) < 0) {
        XLALPrintError("Invalid input parameter: LIGOTimeGPS->gpsNanoSeconds\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }

    /* set up RESOURCE node child (first PARAM) */
    xmlResourceParamNodes[0] = XLALCreateVOTableTypedParamNode(LIGOTimeGPS_gpsSeconds, gpsSecondsBuffer);
    if(!xmlResourceParamNodes[0]) {
        XLALPrintError("Couldn't create PARAM node: gpsSeconds\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up RESOURCE node child (second PARAM) */
    xmlResourceParamNodes[1] = XLALCreateVOTableTypedParamNode(LIGOTimeGPS_gpsNanoSeconds, gpsNanoSecondsBuffer);
    if(!xmlResourceParamNodes[1]) {
        XLALPrintError("Couldn't create PARAM node: gpsNanoSeconds\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up RESOURCE node*/
    xmlResourceNode = XLALCreateVOTableResourceNode("LIGOTimeGPS", name, xmlResourceParamNodes, 2);
    if(!xmlResourceNode) {
        XLALPrintError("Couldn't create RESOURCE node: LIGOTimeGPS\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* return RESOURCE node (needs to be xmlFreeNode'd or xmlFreeDoc'd by caller!!!) */
    return xmlResourceNode;
}


/**
 * \brief Serializes a \c LIGOTimeGPS structure into a VOTable XML string
 *
 * This function takes a \c LIGOTimeGPS structure and serializes it into a full-fledged
 * VOTable XML string containing the serialized structure as the only child element.\n
 * Essentially, this function is just a wrapper for \ref XLALLIGOTimeGPS2VOTableNode and
 * \ref XLALCreateVOTableXMLFromTree followed by a dump of the VOTable document into a
 * string.\n
 *
 * \param ltg [in] Pointer to the \c LIGOTimeGPS structure to be serialized
 * \param name [in] Unique identifier of this particular \c LIGOTimeGPS structure instance
 *
 * \return A pointer to a \c xmlChar (string) that holds the VOTable document containing
 * solely the \c LIGOTimeGPS structure. Please note that the string will be encoded in UTF-8.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * string isn't needed anymore) using \c xmlFree.
 *
 * \sa XLALLIGOTimeGPS2VOTableNode
 * \sa XLALCreateVOTableXMLFromTree
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
xmlChar * XLALLIGOTimeGPS2VOTableXML(const LIGOTimeGPS *const ltg, const char *name)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALLIGOTimeGPS2VOTableXML";
    xmlChar *xmlStringBuffer = NULL;
    INT4 xmlStringBufferSize = -1;
    xmlNodePtr xmlTree;

    /* sanity checks */
    if(!ltg) {
        XLALPrintError("Invalid input parameter: ltg\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }

    /* prepare XML serialization */
    xmlThrDefIndentTreeOutput(1);

    /* build VOTable fragment (tree) */
    xmlTree = XLALLIGOTimeGPS2VOTableNode(ltg, name);
    if(xmlTree == NULL) {
        XLALPrintError("VOTable fragment construction failed\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* retrieve full VOTable XML document (string) */
    if(XLALCreateVOTableStringFromTree(xmlTree, &xmlStringBuffer, &xmlStringBufferSize)) {
        /* clean up */
        xmlCleanupParser();

        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* clean up */
    xmlCleanupParser();

    /* return XML string (needs to be xmlFree'd by caller!!!) */
    return xmlStringBuffer;
}


/**
 * \brief Deserializes a \c LIGOTimeGPS structure from a VOTable XML document
 *
 * This function takes a VOTable XML document and deserializes (extracts)
 * the \c LIGOTimeGPS structure identified by the given name.
 *
 * \param xml [in] Pointer to the VOTable XML document containing the structure
 * \param name [in] Unique identifier of the particular \c LIGOTimeGPS structure to be deserialized
 * \param ltg [out] Pointer to an empty \c  LIGOTimeGPS structure to store the deserialized instance
 *
 * \return \c XLAL_SUCCESS if the specified \c LIGOTimeGPS structure could be found and
 * deserialized successfully.
 *
 * \sa XLALVOTableXML2LIGOTimeGPSByName
 * \sa XLALGetSingleNodeContentByXPath
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
INT4 XLALVOTableDoc2LIGOTimeGPSByName(xmlDocPtr xmlDocument, const char *name, LIGOTimeGPS *ltg)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALVOTableDoc2LIGOTimeGPSByName";
    xmlChar *nodeContent = NULL;
    CHAR xpath[XPATHSTR_MAXLEN] = {0};

    /* sanity checks */
    if(!xmlDocument) {
        XLALPrintError("Invalid input parameter: xmlDocument\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!ltg) {
        XLALPrintError("Invalid input parameter: ltg\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }

    /* prepare XPATH search for LIGOTimeGPS.gpsSeconds */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='LIGOTimeGPS' and @name='%s']/PARAM[@name='gpsSeconds']/@value",
            name) < 0)
    {
        /* clean up */
        XLALPrintError("XPATH statement construction failed: LIGOTimeGPS.gpsSeconds\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve LIGOTimeGPS.gpsSeconds */
    nodeContent = (xmlChar *) XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%i", &ltg->gpsSeconds) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: gpsSeconds\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* prepare XPATH search for LIGOTimeGPS.gpsNanoSeconds */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='LIGOTimeGPS' and @name='%s']/PARAM[@name='gpsNanoSeconds']/@value",
            name) < 0)
    {
        /* clean up */
        xmlFree(nodeContent);
        XLALPrintError("XPATH statement construction failed: LIGOTimeGPS.gpsNanoSeconds\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve LIGOTimeGPS.gpsNanoSeconds */
    nodeContent = (xmlChar *)XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%i", &ltg->gpsNanoSeconds) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: gpsNanoSeconds\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* clean up*/
    xmlFree(nodeContent);

    return XLAL_SUCCESS;
}


/**
 * \brief Deserializes a \c LIGOTimeGPS structure from a VOTable XML string
 *
 * This function takes a VOTable XML document (string) and deserializes (extracts)
 * the \c LIGOTimeGPS structure identified by the given name.
 *
 * \param xml [in] Pointer to the VOTable XML document (string) containing the structure
 * \param name [in] Unique identifier of the particular \c LIGOTimeGPS structure to be deserialized
 * \param ltg [out] Pointer to an empty \c  LIGOTimeGPS structure to store the deserialized instance
 *
 * \return \c XLAL_SUCCESS if the specified \c LIGOTimeGPS structure could be found and
 * deserialized successfully.
 *
 * \sa XLALVOTableDoc2LIGOTimeGPSByName
 * \sa XLALGetSingleNodeContentByXPath
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
INT4 XLALVOTableXML2LIGOTimeGPSByName(const char *xml, const char *name, LIGOTimeGPS *ltg)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALVOTableXML2LIGOTimeGPSByName";
    xmlDocPtr xmlDocument = NULL;
    INT4 result;

    /* sanity checks */
    if(!xml) {
        XLALPrintError("Invalid input parameter: xml\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!ltg) {
        XLALPrintError("Invalid input parameter: ltg\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }

    /* parse XML document */
    xmlDocument = xmlReadMemory(xml, strlen(xml), NULL, "UTF-8", 0);
    if(xmlDocument == NULL) {
        /* clean up */
        xmlCleanupParser();

        XLALPrintError("VOTable document parsing failed\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* run the actual parser */
    result = XLALVOTableDoc2LIGOTimeGPSByName(xmlDocument, name, ltg);

    /* clean up */
    xmlFreeDoc(xmlDocument);
    xmlCleanupParser();

    return result;
}


/**
 * \brief Serializes a \c BinaryOrbitParams structure into a VOTable XML %node
 *
 * This function takes a \c BinaryOrbitParams structure and serializes it into a VOTable
 * \c RESOURCE %node identified by the given name. The returned \c xmlNode can then be
 * embedded into an existing %node hierarchy or turned into a full VOTable document.
 *
 * \param bop [in] Pointer to the \c BinaryOrbitParams structure to be serialized
 * \param name [in] Unique identifier of this particular \c BinaryOrbitParams structure instance
 *
 * \return A pointer to a \c xmlNode that holds the VOTable fragment that represents
 * the \c BinaryOrbitParams structure.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * fragment isn't needed anymore) using \c xmlFreeNode. Alternatively, \c xmlFreeDoc
 * can be used later on when the returned fragment has been embedded in a XML document.
 *
 * \sa XLALCreateVOTableCustomParamNode
 * \sa XLALCreateVOTableResourceNode
 * \sa XLALCreateVOTableXMLFromTree
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
xmlNodePtr XLALBinaryOrbitParams2VOTableNode(const BinaryOrbitParams *const bop, const char *name)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALBinaryOrbitParams2VOTableNode";
    xmlNodePtr xmlParentNode = NULL;
    xmlNodePtr xmlParentChildNodes[5] = {NULL};

    CHAR argp[REAL8STR_MAXLEN] = {0};
    CHAR asini[REAL8STR_MAXLEN] = {0};
    CHAR ecc[REAL8STR_MAXLEN] = {0};
    CHAR period[REAL8STR_MAXLEN] = {0};

    /* make sure that the shared library is the same as the
     * library version the code was compiled against */
    LIBXML_TEST_VERSION

    /* check and prepare input parameters */
    if(!bop || LALSnprintf(argp, REAL8STR_MAXLEN, "%g", bop->argp) < 0) {
        XLALPrintError("Invalid input parameter: PulsarDopplerParams->argp\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!bop || LALSnprintf(asini, REAL8STR_MAXLEN, "%g", bop->asini) < 0) {
        XLALPrintError("Invalid input parameter: PulsarDopplerParams->asini\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!bop || LALSnprintf(ecc, REAL8STR_MAXLEN, "%g", bop->ecc) < 0) {
        XLALPrintError("Invalid input parameter: PulsarDopplerParams->ecc\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!bop || LALSnprintf(period, REAL8STR_MAXLEN, "%g", bop->period) < 0) {
        XLALPrintError("Invalid input parameter: PulsarDopplerParams->period\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }

    /* set up RESOURCE node (refTime)*/
    xmlParentChildNodes[0] = XLALLIGOTimeGPS2VOTableNode(&bop->tp, "tp");
    if(!xmlParentChildNodes[0]) {
        XLALPrintError("Couldn't create RESOURCE node: BinaryOrbitParams.tp\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up PARAM node (argp) */
    xmlParentChildNodes[1] = XLALCreateVOTableTypedParamNode(BinaryOrbitParams_argp, argp);
    if(!xmlParentChildNodes[1]) {
        XLALPrintError("Couldn't create PARAM node: BinaryOrbitParams.argp\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up PARAM node (asini) */
    xmlParentChildNodes[2] = XLALCreateVOTableTypedParamNode(BinaryOrbitParams_asini, asini);
    if(!xmlParentChildNodes[2]) {
        XLALPrintError("Couldn't create PARAM node: BinaryOrbitParams.asini\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up PARAM node (ecc) */
    xmlParentChildNodes[3] = XLALCreateVOTableTypedParamNode(BinaryOrbitParams_ecc, ecc);
    if(!xmlParentChildNodes[3]) {
        XLALPrintError("Couldn't create PARAM node: BinaryOrbitParams.ecc\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up PARAM node (period) */
    xmlParentChildNodes[4] = XLALCreateVOTableTypedParamNode(BinaryOrbitParams_period, period);
    if(!xmlParentChildNodes[4]) {
        XLALPrintError("Couldn't create PARAM node: BinaryOrbitParams.period\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up parent RESOURCE node*/
    xmlParentNode = XLALCreateVOTableResourceNode("BinaryOrbitParams", name, xmlParentChildNodes, 5);
    if(!xmlParentNode) {
        XLALPrintError("Couldn't create RESOURCE node: BinaryOrbitParams\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* return RESOURCE node (needs to be xmlFreeNode'd or xmlFreeDoc'd by caller!!!) */
    return xmlParentNode;
}


/**
 * \brief Serializes a \c BinaryOrbitParams structure into a VOTable XML string
 *
 * This function takes a \c BinaryOrbitParams structure and serializes it into a full-fledged
 * VOTable XML string containing the serialized structure as the only child element.\n
 * Essentially, this function is just a wrapper for \ref XLALBinaryOrbitParams2VOTableNode and
 * \ref XLALCreateVOTableXMLFromTree followed by a dump of the VOTable document into a
 * string.\n
 *
 * \param bop [in] Pointer to the \c BinaryOrbitParams structure to be serialized
 * \param name [in] Unique identifier of this particular \c BinaryOrbitParams structure instance
 *
 * \return A pointer to a \c xmlChar (string) that holds the VOTable document containing
 * solely the \c BinaryOrbitParams structure. Please note that the string will be encoded in UTF-8.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * string isn't needed anymore) using \c xmlFree.
 *
 * \sa XLALBinaryOrbitParams2VOTableNode
 * \sa XLALCreateVOTableXMLFromTree
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
xmlChar * XLALBinaryOrbitParams2VOTableXML(const BinaryOrbitParams *const bop, const char *name)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALBinaryOrbitParams2VOTableXML";
    xmlChar *xmlStringBuffer = NULL;
    INT4 xmlStringBufferSize = -1;
    xmlNodePtr xmlTree;

    /* sanity checks */
    if(!bop) {
        XLALPrintError("Invalid input parameter: bop\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }

    /* prepare XML serialization */
    xmlThrDefIndentTreeOutput(1);

    /* build VOTable fragment (tree) */
    xmlTree = XLALBinaryOrbitParams2VOTableNode(bop, name);
    if(xmlTree == NULL) {
        XLALPrintError("VOTable fragment construction failed\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* retrieve full VOTable XML document (string) */
    if(XLALCreateVOTableStringFromTree(xmlTree, &xmlStringBuffer, &xmlStringBufferSize)) {
        /* clean up */
        xmlCleanupParser();

        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* clean up */
    xmlCleanupParser();

    /* return XML string (needs to be xmlFree'd by caller!!!) */
    return xmlStringBuffer;
}


/**
 * \brief Deserializes a \c BinaryOrbitParams structure from a VOTable XML document
 *
 * This function takes a VOTable XML document and deserializes (extracts)
 * the \c BinaryOrbitParams structure identified by the given name.
 *
 * \param xmlDoc [in] Pointer to the VOTable XML document containing the structure
 * \param name [in] Unique identifier of the particular \c BinaryOrbitParams structure to be deserialized
 * \param bop [out] Pointer to an empty \c  BinaryOrbitParams structure to store the deserialized instance
 *
 * \return \c XLAL_SUCCESS if the specified \c BinaryOrbitParams structure could be found and
 * deserialized successfully.
 *
 * \sa XLALVOTableXML2BinaryOrbitParamsByName
 * \sa XLALGetSingleNodeContentByXPath
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
INT4 XLALVOTableDoc2BinaryOrbitParamsByName(xmlDocPtr xmlDocument, const char *name, BinaryOrbitParams *bop)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALVOTableDoc2BinaryOrbitParamsByName";
    xmlChar *nodeContent = NULL;
    CHAR xpath[XPATHSTR_MAXLEN] = {0};

    /* sanity checks */
    if(!xmlDocument) {
        XLALPrintError("Invalid input parameter: xmlDocument\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!bop) {
        XLALPrintError("Invalid input parameter: bop\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }

    /* retrieve BinaryOrbitParams.tp */
    if(XLALVOTableDoc2LIGOTimeGPSByName(xmlDocument, "tp", &bop->tp)) {
        /* clean up */
        XLALPrintError("Error parsing XML document content: BinaryOrbitParams.tp\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* prepare XPATH search for BinaryOrbitParams.argp */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='BinaryOrbitParams' and @name='%s']/PARAM[@name='argp']/@value",
            name) < 0)
    {
        /* clean up */
        XLALPrintError("XPATH statement construction failed: BinaryOrbitParams.argp\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve BinaryOrbitParams.argp */
    nodeContent = (xmlChar *) XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%lf", &bop->argp) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: BinaryOrbitParams.argp\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* prepare XPATH search for PulsarDopplerParams.asini */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='BinaryOrbitParams' and @name='%s']/PARAM[@name='asini']/@value",
            name) < 0)
    {
        /* clean up */
        xmlFree(nodeContent);
        XLALPrintError("XPATH statement construction failed: BinaryOrbitParams.asini\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve BinaryOrbitParams.Delta */
    nodeContent = (xmlChar *)XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%lf", &bop->asini) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: BinaryOrbitParams.asini\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* prepare XPATH search for BinaryOrbitParams.ecc */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='BinaryOrbitParams' and @name='%s']/PARAM[@name='ecc']/@value",
            name) < 0)
    {
        /* clean up */
        xmlFree(nodeContent);
        XLALPrintError("XPATH statement construction failed: BinaryOrbitParams.ecc\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve PulsarDopplerParams.ecc */
    nodeContent = (xmlChar *)XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%lf", &bop->ecc) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: BinaryOrbitParams.ecc\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* prepare XPATH search for BinaryOrbitParams.period */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='BinaryOrbitParams' and @name='%s']/PARAM[@name='period']/@value",
            name) < 0)
    {
        /* clean up */
        xmlFree(nodeContent);
        XLALPrintError("XPATH statement construction failed: BinaryOrbitParams.period\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve PulsarDopplerParams.fkdot */
    nodeContent = (xmlChar *)XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%lf", &bop->period) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: BinaryOrbitParams.period\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* clean up*/
    xmlFree(nodeContent);

    return XLAL_SUCCESS;
}


/**
 * \brief Deserializes a \c BinaryOrbitParams structure from a VOTable XML string
 *
 * This function takes a VOTable XML document (string) and deserializes (extracts)
 * the \c BinaryOrbitParams structure identified by the given name.
 *
 * \param xml [in] Pointer to the VOTable XML document (string) containing the structure
 * \param name [in] Unique identifier of the particular \c BinaryOrbitParams structure to be deserialized
 * \param bop [out] Pointer to an empty \c  BinaryOrbitParams structure to store the deserialized instance
 *
 * \return \c XLAL_SUCCESS if the specified \c BinaryOrbitParams structure could be found and
 * deserialized successfully.
 *
 * \sa XLALVOTableDoc2BinaryOrbitParamsByName
 * \sa XLALGetSingleNodeContentByXPath
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
INT4 XLALVOTableXML2BinaryOrbitParamsByName(const char *xml, const char *name, BinaryOrbitParams *bop)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALVOTableXML2BinaryOrbitParamsByName";
    xmlDocPtr xmlDocument = NULL;
    INT4 result;

    /* sanity checks */
    if(!xml) {
        XLALPrintError("Invalid input parameter: xml\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!bop) {
        XLALPrintError("Invalid input parameter: bop\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }

    /* parse XML document */
    xmlDocument = xmlReadMemory(xml, strlen(xml), NULL, "UTF-8", 0);
    if(xmlDocument == NULL) {
        /* clean up */
        xmlCleanupParser();

        XLALPrintError("VOTable document parsing failed\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* run the actual parser */
    result = XLALVOTableDoc2BinaryOrbitParamsByName(xmlDocument, name, bop);

    /* clean up */
    xmlFreeDoc(xmlDocument);
    xmlCleanupParser();

    return result;
}


/**
 * \brief Serializes a \c PulsarDopplerParams structure into a VOTable XML %node
 *
 * This function takes a \c PulsarDopplerParams structure and serializes it into a VOTable
 * \c RESOURCE %node identified by the given name. The returned \c xmlNode can then be
 * embedded into an existing %node hierarchy or turned into a full VOTable document.
 *
 * \param pdp [in] Pointer to the \c PulsarDopplerParams structure to be serialized
 * \param name [in] Unique identifier of this particular \c PulsarDopplerParams structure instance
 *
 * \return A pointer to a \c xmlNode that holds the VOTable fragment that represents
 * the \c PulsarDopplerParams structure.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * fragment isn't needed anymore) using \c xmlFreeNode. Alternatively, \c xmlFreeDoc
 * can be used later on when the returned fragment has been embedded in a XML document.
 *
 * \sa XLALCreateVOTableCustomParamNode
 * \sa XLALCreateVOTableResourceNode
 * \sa XLALCreateVOTableXMLFromTree
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
xmlNodePtr XLALPulsarDopplerParams2VOTableNode(const PulsarDopplerParams *const pdp, const char *name)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALPulsarDopplerParams2VOTableNode";
    xmlNodePtr xmlParentNode = NULL;
    xmlNodePtr xmlParentChildNodes[5] = {NULL};
    int i;

    CHAR Alpha[REAL8STR_MAXLEN] = {0};
    CHAR Delta[REAL8STR_MAXLEN] = {0};
    CHAR fkdot[PULSAR_MAX_SPINS*REAL8STR_MAXLEN] = {0};
    CHAR fkDotTemp[REAL8STR_MAXLEN] = {0};

    /* make sure that the shared library is the same as the
     * library version the code was compiled against */
    LIBXML_TEST_VERSION

    /* check and prepare input parameters */
    if(!pdp || LALSnprintf(Alpha, REAL8STR_MAXLEN, "%g", pdp->Alpha) < 0) {
        XLALPrintError("Invalid input parameter: PulsarDopplerParams->Alpha\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!pdp || LALSnprintf(Delta, REAL8STR_MAXLEN, "%g", pdp->Delta) < 0) {
        XLALPrintError("Invalid input parameter: PulsarDopplerParams->Delta\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    for(i = 0; i < PULSAR_MAX_SPINS; ++i) {
        if(!pdp || LALSnprintf(fkDotTemp, REAL8STR_MAXLEN, "%g", pdp->fkdot[i]) < 0) {
            XLALPrintError("Invalid input parameter: PulsarDopplerParams->fkdot[%i]\n", i);
            XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
        }
        if(!strncat(fkdot, fkDotTemp, REAL8STR_MAXLEN)) {
            XLALPrintError("Couldn't serialize parameter: PulsarDopplerParams->fkdot[%i]\n", i);
            XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
        }
        /* add delimiter (SPACE)*/
        if(i<PULSAR_MAX_SPINS-1 && !strncat(fkdot, " ", 1)) {
            XLALPrintError("Couldn't add delimiter to parameter: PulsarDopplerParams->fkdot[%i]\n", i);
            XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
        }
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }

    /* set up RESOURCE node (refTime)*/
    xmlParentChildNodes[0] = XLALLIGOTimeGPS2VOTableNode(&pdp->refTime, "refTime");
    if(!xmlParentChildNodes[0]) {
        XLALPrintError("Couldn't create RESOURCE node: PulsarDopplerParams.refTime\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up PARAM node (Alpha) */
    xmlParentChildNodes[1] = XLALCreateVOTableTypedParamNode(PulsarDopplerParams_Alpha, Alpha);
    if(!xmlParentChildNodes[1]) {
        XLALPrintError("Couldn't create PARAM node: PulsarDopplerParams.Alpha\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up PARAM node (Delta) */
    xmlParentChildNodes[2] = XLALCreateVOTableTypedParamNode(PulsarDopplerParams_Delta, Delta);
    if(!xmlParentChildNodes[2]) {
        XLALPrintError("Couldn't create PARAM node: PulsarDopplerParams.Delta\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up PARAM node (fkdot) */
    xmlParentChildNodes[3] = XLALCreateVOTableTypedParamNode(PulsarDopplerParams_fkdot, fkdot);
    if(!xmlParentChildNodes[3]) {
        XLALPrintError("Couldn't create PARAM node: PulsarDopplerParams.fkdot\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up RESOURCE node (orbit)*/
    xmlParentChildNodes[4] = XLALBinaryOrbitParams2VOTableNode(pdp->orbit, "orbit");
    if(!xmlParentChildNodes[4]) {
        XLALPrintError("Couldn't create RESOURCE node: PulsarDopplerParams.orbit\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* set up parent RESOURCE node*/
    xmlParentNode = XLALCreateVOTableResourceNode("PulsarDopplerParams", name, xmlParentChildNodes, 5);
    if(!xmlParentNode) {
        XLALPrintError("Couldn't create RESOURCE node: PulsarDopplerParams\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* return RESOURCE node (needs to be xmlFreeNode'd or xmlFreeDoc'd by caller!!!) */
    return xmlParentNode;
}


/**
 * \brief Serializes a \c PulsarDopplerParams structure into a VOTable XML string
 *
 * This function takes a \c PulsarDopplerParams structure and serializes it into a full-fledged
 * VOTable XML string containing the serialized structure as the only child element.\n
 * Essentially, this function is just a wrapper for \ref XLALPulsarDopplerParams2VOTableNode and
 * \ref XLALCreateVOTableXMLFromTree followed by a dump of the VOTable document into a
 * string.\n
 *
 * \param pdp [in] Pointer to the \c PulsarDopplerParams structure to be serialized
 * \param name [in] Unique identifier of this particular \c PulsarDopplerParams structure instance
 *
 * \return A pointer to a \c xmlChar (string) that holds the VOTable document containing
 * solely the \c PulsarDopplerParams structure. Please note that the string will be encoded in UTF-8.
 * In case of an error, a null-pointer is returned.\n
 * \b Important: the caller is responsible to free the allocated memory (when the
 * string isn't needed anymore) using \c xmlFree.
 *
 * \sa XLALPulsarDopplerParams2VOTableNode
 * \sa XLALCreateVOTableXMLFromTree
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
xmlChar * XLALPulsarDopplerParams2VOTableXML(const PulsarDopplerParams *const pdp, const char *name)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALPulsarDopplerParams2VOTableXML";
    xmlChar *xmlStringBuffer = NULL;
    INT4 xmlStringBufferSize = -1;
    xmlNodePtr xmlTree;

    /* sanity checks */
    if(!pdp) {
        XLALPrintError("Invalid input parameter: pdp\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR_NULL(logReference, XLAL_EINVAL);
    }

    /* prepare XML serialization */
    xmlThrDefIndentTreeOutput(1);

    /* build VOTable fragment (tree) */
    xmlTree = XLALPulsarDopplerParams2VOTableNode(pdp, name);
    if(xmlTree == NULL) {
        XLALPrintError("VOTable fragment construction failed\n");
        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* retrieve full VOTable XML document (string) */
    if(XLALCreateVOTableStringFromTree(xmlTree, &xmlStringBuffer, &xmlStringBufferSize)) {
        /* clean up */
        xmlCleanupParser();

        XLAL_ERROR_NULL(logReference, XLAL_EFAILED);
    }

    /* clean up */
    xmlCleanupParser();

    /* return XML string (needs to be xmlFree'd by caller!!!) */
    return xmlStringBuffer;
}


/**
 * \brief Deserializes a \c PulsarDopplerParams structure from a VOTable XML document
 *
 * This function takes a VOTable XML document and deserializes (extracts)
 * the \c PulsarDopplerParams structure identified by the given name.
 *
 * \param xmlDoc [in] Pointer to the VOTable XML document containing the structure
 * \param name [in] Unique identifier of the particular \c PulsarDopplerParams structure to be deserialized
 * \param pdp [out] Pointer to an empty \c  PulsarDopplerParams structure to store the deserialized instance
 *
 * \return \c XLAL_SUCCESS if the specified \c PulsarDopplerParams structure could be found and
 * deserialized successfully.
 *
 * \sa XLALVOTableXML2PulsarDopplerParamsByName
 * \sa XLALGetSingleNodeContentByXPath
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
INT4 XLALVOTableDoc2PulsarDopplerParamsByName(xmlDocPtr xmlDocument, const char *name, PulsarDopplerParams *pdp)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALVOTableDoc2PulsarDopplerParamsByName";
    xmlChar *nodeContent = NULL;
    CHAR xpath[XPATHSTR_MAXLEN] = {0};

    /* sanity checks */
    if(!xmlDocument) {
        XLALPrintError("Invalid input parameter: xmlDocument\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!pdp) {
        XLALPrintError("Invalid input parameter: pdp\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }

    /* retrieve PulsarDopplerParams.refTime */
    if(XLALVOTableDoc2LIGOTimeGPSByName(xmlDocument, "refTime", &pdp->refTime)) {
        /* clean up */
        XLALPrintError("Error parsing XML document content: PulsarDopplerParams.refTime\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* prepare XPATH search for PulsarDopplerParams.Alpha */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='PulsarDopplerParams' and @name='%s']/PARAM[@name='Alpha']/@value",
            name) < 0)
    {
        /* clean up */
        XLALPrintError("XPATH statement construction failed: PulsarDopplerParams.Alpha\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve PulsarDopplerParams.Alpha */
    nodeContent = (xmlChar *) XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%lf", &pdp->Alpha) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: PulsarDopplerParams.Alpha\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* prepare XPATH search for PulsarDopplerParams.Delta */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='PulsarDopplerParams' and @name='%s']/PARAM[@name='Delta']/@value",
            name) < 0)
    {
        /* clean up */
        xmlFree(nodeContent);
        XLALPrintError("XPATH statement construction failed: PulsarDopplerParams.Delta\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve PulsarDopplerParams.Delta */
    nodeContent = (xmlChar *)XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%lf", &pdp->Delta) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: PulsarDopplerParams.Delta\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* prepare XPATH search for PulsarDopplerParams.fkdot */
    if(LALSnprintf(
            xpath,
            XPATHSTR_MAXLEN,
            "//RESOURCE[@utype='PulsarDopplerParams' and @name='%s']/PARAM[@name='fkdot']/@value",
            name) < 0)
    {
        /* clean up */
        xmlFree(nodeContent);
        XLALPrintError("XPATH statement construction failed: PulsarDopplerParams.fkdot\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* retrieve PulsarDopplerParams.fkdot */
    nodeContent = (xmlChar *)XLALGetSingleNodeContentByXPath(xmlDocument, xpath);

    /* parse and finally store content */
    if(!nodeContent || sscanf((char*)nodeContent, "%lf %lf %lf %lf", &pdp->fkdot[0], &pdp->fkdot[1], &pdp->fkdot[2], &pdp->fkdot[3]) == EOF) {
        /* clean up*/
        xmlFree(nodeContent);
        XLALPrintError("Invalid node content encountered: PulsarDopplerParams.fkdot\n");
        XLAL_ERROR(logReference, XLAL_EDATA);
    }

    /* retrieve PulsarDopplerParams.orbit */
    if(XLALVOTableDoc2BinaryOrbitParamsByName(xmlDocument, "orbit", pdp->orbit)) {
        /* clean up */
        XLALPrintError("Error parsing XML document content: PulsarDopplerParams.orbit\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* clean up*/
    xmlFree(nodeContent);

    return XLAL_SUCCESS;
}


/**
 * \brief Deserializes a \c PulsarDopplerParams structure from a VOTable XML string
 *
 * This function takes a VOTable XML document (string) and deserializes (extracts)
 * the \c PulsarDopplerParams structure identified by the given name.
 *
 * \param xml [in] Pointer to the VOTable XML document (string) containing the structure
 * \param name [in] Unique identifier of the particular \c PulsarDopplerParams structure to be deserialized
 * \param pdp [out] Pointer to an empty \c  PulsarDopplerParams structure to store the deserialized instance
 *
 * \return \c XLAL_SUCCESS if the specified \c PulsarDopplerParams structure could be found and
 * deserialized successfully.
 *
 * \sa XLALVOTableDoc2PulsarDopplerParamsByName
 * \sa XLALGetSingleNodeContentByXPath
 *
 * \author Oliver Bock\n
 * Albert-Einstein-Institute Hannover, Germany
 */
INT4 XLALVOTableXML2PulsarDopplerParamsByName(const char *xml, const char *name, PulsarDopplerParams *pdp)
{
    /* set up local variables */
    static const CHAR *logReference = "XLALVOTableXML2PulsarDopplerParamsByName";
    xmlDocPtr xmlDocument = NULL;
    INT4 result;

    /* sanity checks */
    if(!xml) {
        XLALPrintError("Invalid input parameter: xml\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!name || strlen(name) <= 0) {
        XLALPrintError("Invalid input parameter: name\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }
    if(!pdp) {
        XLALPrintError("Invalid input parameter: pdp\n");
        XLAL_ERROR(logReference, XLAL_EINVAL);
    }

    /* parse XML document */
    xmlDocument = xmlReadMemory(xml, strlen(xml), NULL, "UTF-8", 0);
    if(xmlDocument == NULL) {
        /* clean up */
        xmlCleanupParser();

        XLALPrintError("VOTable document parsing failed\n");
        XLAL_ERROR(logReference, XLAL_EFAILED);
    }

    /* run the actual parser */
    result = XLALVOTableDoc2PulsarDopplerParamsByName(xmlDocument, name, pdp);

    /* clean up */
    xmlFreeDoc(xmlDocument);
    xmlCleanupParser();

    return result;
}
