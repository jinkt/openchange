/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007-2008.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libmapi/libmapi.h>
#include <libmapi/proto_private.h>


/**
   \file IABContainer.c

   \brief Provides access to address book containers -- Used to
   perform name resolution
*/


/**
   \details Resolve user names against the Windows Address Book Provider

   \param session pointer to the MAPI session context
   \param usernames list of user names to resolve
   \param rowset resulting list of user details
   \param props resulting list of resolved names
   \param flaglist resulting list of resolution status (see below)
   \param flags if set to MAPI_UNICODE then UNICODE MAPITAGS can be
   used, otherwise only UTF8 encoded fields may be returned.

   Possible flaglist values are:
   - MAPI_UNRESOLVED: could not be resolved
   - MAPI_AMBIGUOUS: resolution match more than one entry
   - MAPI_RESOLVED: resolution matched a single entry
 
   \return MAPI_E_SUCCESS on success, otherwise -1.
   
   \note Developers should call GetLastError() to retrieve the last MAPI error
   code. Possible MAPI error codes are:
   - MAPI_E_NOT_INITIALIZED: MAPI subsystem has not been initialized
   - MAPI_E_SESSION_LIMIT: No session has been opened on the provider
   - MAPI_E_NOT_ENOUGH_RESOURCES: MAPI subsystem failed to allocate
     the necessary resources to operate properly
   - MAPI_E_NOT_FOUND: No suitable profile database was found in the
     path pointed by profiledb
   - MAPI_E_CALL_FAILED: A network problem was encountered during the
     transaction
   
   \sa MAPILogonProvider, GetLastError
 */
_PUBLIC_ enum MAPISTATUS ResolveNames(struct mapi_session *session,
				      const char **usernames, 
				      struct SPropTagArray *props, 
				      struct SRowSet **rowset, 
				      struct SPropTagArray **flaglist, 
				      uint32_t flags)
{
	struct nspi_context	*nspi;
	enum MAPISTATUS		retval;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!rowset, MAPI_E_INVALID_PARAMETER, NULL);

	nspi = (struct nspi_context *)session->nspi->ctx;

	*rowset = talloc_zero(session, struct SRowSet);
	*flaglist = talloc_zero(session, struct SPropTagArray);

	switch (flags) {
	case MAPI_UNICODE:
		retval = nspi_ResolveNamesW(nspi, usernames, props, &rowset, &flaglist);
		break;
	default:
		retval = nspi_ResolveNames(nspi, usernames, props, &rowset, &flaglist);
		break;
	}

	if (retval != MAPI_E_SUCCESS) return retval;

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve the global address list
   
   \param session pointer to the MAPI session context
   \param SPropTagArray pointer on an array of MAPI properties we want
   to fetch
   \param SRowSet pointer on the rows returned
   \param count the number of rows we want to fetch
   \param ulFlags specify the table cursor location

   Possible value for ulFlags:
   - TABLE_START: Fetch rows from the beginning of the table
   - TABLE_CUR: Fetch rows from current table location

   \return MAPI_E_SUCCESS on success, otherwise -1.

   \sa MapiLogonEx, MapiLogonProvider
 */
_PUBLIC_ enum MAPISTATUS GetGALTable(struct mapi_session *session,
				     struct SPropTagArray *SPropTagArray, 
				     struct SRowSet **SRowSet, 
				     uint32_t count, 
				     uint8_t ulFlags)
{
	struct nspi_context	*nspi;
	struct SRowSet		*srowset;
	enum MAPISTATUS		retval;

	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!SRowSet, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!SPropTagArray, MAPI_E_INVALID_PARAMETER, NULL);

	nspi = (struct nspi_context *)session->nspi->ctx;

	if (ulFlags == TABLE_START) {
		nspi->pStat->CurrentRec = 0;
		nspi->pStat->Delta = 0;
		nspi->pStat->NumPos = 0;
		nspi->pStat->TotalRecs = 0xffffffff;
	}

	srowset = talloc_zero(session, struct SRowSet);
	retval = nspi_QueryRows(nspi, SPropTagArray, NULL, count, &srowset);
	*SRowSet = srowset;

	if (retval != MAPI_E_SUCCESS) return retval;

	return MAPI_E_SUCCESS;
}


/**
   \details Retrieve Address Book information for a given recipient

   \param session pointer to the MAPI session context
   \param username pointer to the username to retrieve information from
   \param pPropTags pointer to the property tags array to lookup
   \param SRowSet pointer on pointer to the results

   Note that if pPropTags is NULL, then GetABNameInfo will fetch
   the following default property tags:
   -# PR_ADDRTYPE_UNICODE
   -# PR_EMAIL_ADDRESS_UNICODE
   -# PR_DISPLAY_NAME_UNICODE
   -# PR_OBJECT_TYPE

   \return MAPI_E_SUCCESS on success, otherwise MAPI error. Possible
   MAPI errors are:
   -# MAPI_E_NOT_INITIALIZED if MAPI subsystem is not initialized
   -# MAPI_E_SESSION_LIMIT if the NSPI session is unavailable
   -# MAPI_E_INVALID_PARAMETER if a function parameter is invalid
   -# MAPI_E_NOT_FOUND if the username to lookup doesn't match any
      records

   \sa nspi_DNToMId, nspi_GetProps
 */
_PUBLIC_ enum MAPISTATUS GetABRecipientInfo(struct mapi_session *session,
				       const char *username,
				       struct SPropTagArray *pPropTags,
				       struct SRowSet **ppRowSet)
{
	enum MAPISTATUS		retval;
	TALLOC_CTX		*mem_ctx;
	struct nspi_context	*nspi_ctx;
	struct SRowSet		*SRowSet;
	struct SPropTagArray	*SPropTagArray = NULL;
	struct SPropTagArray	*pMId = NULL;
	struct SPropTagArray   	*flaglist = NULL;
	struct StringsArray_r	pNames;
	const char		*usernames[2];
	char			*email = NULL;
	bool			allocated = false;

	/* Sanity checks */
	MAPI_RETVAL_IF(!global_mapi_ctx, MAPI_E_NOT_INITIALIZED, NULL);
	MAPI_RETVAL_IF(!session, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!session->profile, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!session->nspi, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!session->nspi->ctx, MAPI_E_SESSION_LIMIT, NULL);
	MAPI_RETVAL_IF(!ppRowSet, MAPI_E_INVALID_PARAMETER, NULL);
	MAPI_RETVAL_IF(!username, MAPI_E_INVALID_PARAMETER, NULL);

	nspi_ctx = (struct nspi_context *)session->nspi->ctx;
	mem_ctx = nspi_ctx->mem_ctx;

	/* Step 1. Resolve the username */
	usernames[0] = username;
	usernames[1] = NULL;

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	SPropTagArray = set_SPropTagArray(mem_ctx, 0xc,
					  PR_ENTRYID,
					  PR_DISPLAY_NAME_UNICODE,
					  PR_ADDRTYPE_UNICODE,
					  PR_OBJECT_TYPE,
					  PR_DISPLAY_TYPE,
					  PR_EMAIL_ADDRESS_UNICODE,
					  PR_SEND_INTERNET_ENCODING,
					  PR_SEND_RICH_INFO,
					  PR_SEARCH_KEY,
					  PR_TRANSMITTABLE_DISPLAY_NAME_UNICODE,
					  PR_7BIT_DISPLAY_NAME_UNICODE,
					  PR_SMTP_ADDRESS_UNICODE);
	retval = ResolveNames(session, usernames, SPropTagArray, &SRowSet, &flaglist, MAPI_UNICODE);
	MAPIFreeBuffer(SPropTagArray);
	MAPI_RETVAL_IF(retval, retval, SRowSet);

	if (flaglist->aulPropTag[0] != MAPI_RESOLVED) {
		MAPIFreeBuffer(SRowSet);
		return MAPI_E_NOT_FOUND;
	}

	username = (const char *) get_SPropValue_SRowSet_data(SRowSet, PR_7BIT_DISPLAY_NAME_UNICODE);
	email = talloc_strdup(mem_ctx, (const char *) get_SPropValue_SRowSet_data(SRowSet, PR_EMAIL_ADDRESS_UNICODE));
	MAPIFreeBuffer(SRowSet);

	/* Step 2. Map recipient DN to MId */
	pNames.Count = 0x1;
	pNames.Strings = (const char **) talloc_array(mem_ctx, char **, 1);
	pNames.Strings[0] = email;
	pMId = talloc_zero(mem_ctx, struct SPropTagArray);
	retval = nspi_DNToMId(nspi_ctx, &pNames, &pMId);
	MAPIFreeBuffer((char *)pNames.Strings[0]);
	MAPIFreeBuffer((char **)pNames.Strings);
	MAPI_RETVAL_IF(retval, retval, pMId);

	/* Step 3. Get recipient's properties */
	if (!pPropTags) {
		allocated = true;
		SPropTagArray = set_SPropTagArray(mem_ctx, 0x4,
						  PR_ADDRTYPE_UNICODE,
						  PR_EMAIL_ADDRESS_UNICODE,
						  PR_DISPLAY_NAME_UNICODE,
						  PR_OBJECT_TYPE);
	} else {
		SPropTagArray = pPropTags;
	}

	SRowSet = talloc_zero(mem_ctx, struct SRowSet);
	retval = nspi_GetProps(nspi_ctx, SPropTagArray, pMId, &SRowSet);
	if (allocated == true) {
		MAPIFreeBuffer(SPropTagArray);
	}
	MAPIFreeBuffer(pMId);
	MAPI_RETVAL_IF(retval, retval, SRowSet);

	*ppRowSet = SRowSet;

	return MAPI_E_SUCCESS;
}
