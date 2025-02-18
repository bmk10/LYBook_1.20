/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "StdAfx.h"
#include <AzToolsFramework/AssetDatabase/AssetDatabaseConnection.h>

#include <AzCore/IO/SystemFile.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/SQLite/SQLiteConnection.h>
#include <AzToolsFramework/API/AssetDatabaseBus.h>
#include <AzToolsFramework/Debug/TraceContext.h>
#include <AzToolsFramework/SQLite/SQLiteQuery.h>
#include <AzToolsFramework/SQLite/SQLiteBoundColumnSet.h>

namespace AzToolsFramework
{
    namespace AssetDatabase
    {
        using namespace AzFramework;
        using namespace AzToolsFramework::SQLite;

        //since we can be derived from, prefix the namespace to avoid statement name collisions
        namespace
        {
            static const char* LOG_NAME = "AzToolsFramework::AssetDatabase";

            // when you add a table, be sure to add it here to check the database for corruption
            static const char* EXPECTED_TABLES[] = {
                "BuilderInfo",
                "Files",
                "Jobs",
                "LegacySubIDs",
                "ProductDependencies",
                "Products",
                "ScanFolders",
                "SourceDependency",
                "Sources",
                "dbinfo"
            };

            //////////////////////////////////////////////////////////////////////////
            //table queries
            static const char* QUERY_DATABASEINFO_TABLE = "AzToolsFramework::AssetDatabase::QueryDatabaseInfoTable";
            static const char* QUERY_DATABASEINFO_TABLE_STATEMENT =
                "SELECT * from dbinfo;";
            static const auto s_queryDatabaseinfoTable = MakeSqlQuery(QUERY_DATABASEINFO_TABLE, QUERY_DATABASEINFO_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_BUILDERINFO_TABLE = "AzToolsFramework::AssetDatabase::QueryBuilderInfo";
            static const char* QUERY_BUILDERINFO_TABLE_STATEMENT =
                "SELECT * from BuilderInfo;";
            static const auto s_queryBuilderInfoTable = MakeSqlQuery(QUERY_BUILDERINFO_TABLE, QUERY_BUILDERINFO_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_SCANFOLDERS_TABLE = "AzToolsFramework::AssetDatabase::QueryScanFoldersTable";
            static const char* QUERY_SCANFOLDERS_TABLE_STATEMENT =
                "SELECT * from ScanFolders;";

            static const auto s_queryScanfoldersTable = MakeSqlQuery(QUERY_SCANFOLDERS_TABLE, QUERY_SCANFOLDERS_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_SOURCES_TABLE = "AzToolsFramework::AssetDatabase::QuerySourcesTable";
            static const char* QUERY_SOURCES_TABLE_STATEMENT =
                "SELECT * from Sources;";

            static const auto s_querySourcesTable = MakeSqlQuery(QUERY_SOURCES_TABLE, QUERY_SOURCES_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_JOBS_TABLE = "AzToolsFramework::AssetDatabase::QueryJobsTable";
            static const char* QUERY_JOBS_TABLE_STATEMENT =
                "SELECT * from Jobs;";

            static const auto s_queryJobsTable = MakeSqlQuery(QUERY_JOBS_TABLE, QUERY_JOBS_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_JOBS_TABLE_PLATFORM = "AzToolsFramework::AssetDatabase::QueryJobsTablePlatform";
            static const char* QUERY_JOBS_TABLE_PLATFORM_STATEMENT =
                "SELECT * from Jobs WHERE "
                "Platform = :platform;";

            static const auto s_queryJobsTablePlatform = MakeSqlQuery(QUERY_JOBS_TABLE_PLATFORM, QUERY_JOBS_TABLE_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCTS_TABLE = "AzToolsFramework::AssetDatabase::QueryProductsTable";
            static const char* QUERY_PRODUCTS_TABLE_STATEMENT =
                "SELECT * from Products INNER JOIN Jobs "
                "ON Products.JobPK = Jobs.JobID;";

            static const auto s_queryProductsTable = MakeSqlQuery(QUERY_PRODUCTS_TABLE, QUERY_PRODUCTS_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_PRODUCTS_TABLE_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductsTablePlatform";
            static const char* QUERY_PRODUCTS_TABLE_PLATFORM_STATEMENT =
                "SELECT * from Products INNER JOIN Jobs "
                "ON Products.JobPK = Jobs.JobID WHERE "
                "Jobs.Platform = :platform;";

            static const auto s_queryProductsTablePlatform = MakeSqlQuery(QUERY_PRODUCTS_TABLE_PLATFORM, QUERY_PRODUCTS_TABLE_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_LEGACYSUBIDSBYPRODUCTID = "AzToolsFramework::AssetDatabase::QueryLegacySubIDsByProductID";
            static const char* QUERY_LEGACYSUBIDSBYPRODUCTID_STATEMENT =
                "SELECT * from LegacySubIDs "
                " WHERE "
                "   LegacySubIDs.ProductPK = :productId;";

            static const auto s_queryLegacysubidsbyproductid = MakeSqlQuery(QUERY_LEGACYSUBIDSBYPRODUCTID, QUERY_LEGACYSUBIDSBYPRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productId"));

            static const char* QUERY_PRODUCTDEPENDENCIES_TABLE = "AzToolsFramework::AssetDatabase::QueryProductDependencies";
            static const char* QUERY_PRODUCTDEPENDENCIES_TABLE_STATEMENT =
                "SELECT ProductDependencies.*, SourceGUID, SubID FROM ProductDependencies "
                "INNER JOIN Products ON ProductPK = ProductID "
                "INNER JOIN Jobs ON JobPK = JobID "
                "INNER JOIN Sources ON SourcePK = SourceID;";

            static const auto s_queryProductdependenciesTable = MakeSqlQuery(QUERY_PRODUCTDEPENDENCIES_TABLE, QUERY_PRODUCTDEPENDENCIES_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_FILES_TABLE = "AzToolsFramework::AssetDatabase::QueryFilesTable";
            static const char* QUERY_FILES_TABLE_STATEMENT =
                "SELECT * from Files;";

            static const auto s_queryFilesTable = MakeSqlQuery(QUERY_FILES_TABLE, QUERY_FILES_TABLE_STATEMENT, LOG_NAME);

            //////////////////////////////////////////////////////////////////////////
            //projection and combination queries

            // lookup by primary key
            static const char* QUERY_SCANFOLDER_BY_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QueryScanfolderByScanfolderID";
            static const char* QUERY_SCANFOLDER_BY_SCANFOLDERID_STATEMENT =
                "SELECT * FROM ScanFolders WHERE "
                "ScanFolderID = :scanfolderid;";

            static const auto s_queryScanfolderByScanfolderid = MakeSqlQuery(QUERY_SCANFOLDER_BY_SCANFOLDERID, QUERY_SCANFOLDER_BY_SCANFOLDERID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_SCANFOLDER_BY_DISPLAYNAME = "AzToolsFramework::AssetDatabase::QueryScanfolderByDisplayName";
            static const char* QUERY_SCANFOLDER_BY_DISPLAYNAME_STATEMENT =
                "SELECT * FROM ScanFolders WHERE "
                "DisplayName = :displayname;";

            static const auto s_queryScanfolderByDisplayname = MakeSqlQuery(QUERY_SCANFOLDER_BY_DISPLAYNAME, QUERY_SCANFOLDER_BY_DISPLAYNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":displayname"));

            static const char* QUERY_SCANFOLDER_BY_PORTABLEKEY = "AzToolsFramework::AssetDatabase::QueryScanfolderByPortableKey";
            static const char* QUERY_SCANFOLDER_BY_PORTABLEKEY_STATEMENT =
                "SELECT * FROM ScanFolders WHERE "
                "PortableKey = :portablekey;";

            static const auto s_queryScanfolderByPortablekey = MakeSqlQuery(QUERY_SCANFOLDER_BY_PORTABLEKEY, QUERY_SCANFOLDER_BY_PORTABLEKEY_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":portablekey"));

            // lookup by primary key
            static const char* QUERY_SOURCE_BY_SOURCEID = "AzToolsFramework::AssetDatabase::QuerySourceBySourceID";
            static const char* QUERY_SOURCE_BY_SOURCEID_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceID = :sourceid;";

            static const auto s_querySourceBySourceid = MakeSqlQuery(QUERY_SOURCE_BY_SOURCEID, QUERY_SOURCE_BY_SOURCEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"));

            static const char* QUERY_SOURCE_BY_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QuerySourceByScanFolderID";
            static const char* QUERY_SOURCE_BY_SCANFOLDERID_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "ScanFolderPK = :scanfolderid;";

            static const auto s_querySourceByScanfolderid = MakeSqlQuery(QUERY_SOURCE_BY_SCANFOLDERID, QUERY_SOURCE_BY_SCANFOLDERID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_SOURCE_BY_SOURCEGUID = "AzToolsFramework::AssetDatabase::QuerySourceBySourceGuid";
            static const char* QUERY_SOURCE_BY_SOURCEGUID_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceGuid = :sourceguid;";

            static const auto s_querySourceBySourceguid = MakeSqlQuery(QUERY_SOURCE_BY_SOURCEGUID, QUERY_SOURCE_BY_SOURCEGUID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::Uuid>(":sourceguid"));

            static const char* QUERY_SOURCE_BY_SOURCENAME = "AzToolsFramework::AssetDatabase::QuerySourceBySourceName";
            static const char* QUERY_SOURCE_BY_SOURCENAME_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceName = :sourcename;";

            static const auto s_querySourceBySourcename = MakeSqlQuery(QUERY_SOURCE_BY_SOURCENAME, QUERY_SOURCE_BY_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            static const char* QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QuerySourceBySourceNameScanFolderID";
            static const char* QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceName = :sourcename AND "
                "ScanFolderPK = :scanfolderid;";
            static const auto s_querySourceBySourcenameScanfolderid = MakeSqlQuery(QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID, QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID_STATEMENT, LOG_NAME,
                SqlParam<const char*>(":sourcename"),
                SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_SOURCE_ANALYSISFINGERPRINT = "AzToolsFramework::AssetDatabase::QuerySourceFingerprint";
            static const char* QUERY_SOURCE_ANALYSISFINGERPRINT_STATEMENT =
                "SELECT AnalysisFingerprint FROM Sources WHERE "
                "SourceName = :sourcename AND "
                "ScanFolderPK = :scanfolderid;";
            static const auto s_querySourceAnalysisFingerprint = MakeSqlQuery(QUERY_SOURCE_ANALYSISFINGERPRINT, QUERY_SOURCE_ANALYSISFINGERPRINT_STATEMENT, LOG_NAME,
                SqlParam<const char*>(":sourcename"),
                SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_SOURCES_AND_SCANFOLDERS = "AzToolsFramework::AssetDatabase::QuerySourcesAndScanfolders";
            static const char* QUERY_SOURCES_AND_SCANFOLDERS_STATEMENT =
                "SELECT * FROM Sources "
                "LEFT OUTER JOIN ScanFolders ON ScanFolderPK = ScanFolderID;";

            static const auto s_querySourcesAndScanfolders = MakeSqlQuery(QUERY_SOURCES_AND_SCANFOLDERS, QUERY_SOURCES_AND_SCANFOLDERS_STATEMENT, LOG_NAME);

            static const char* QUERY_SOURCE_LIKE_SOURCENAME = "AzToolsFramework::AssetDatabase::QuerySourceLikeSourceName";
            static const char* QUERY_SOURCE_LIKE_SOURCENAME_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_querySourceLikeSourcename = MakeSqlQuery(QUERY_SOURCE_LIKE_SOURCENAME, QUERY_SOURCE_LIKE_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            // lookup by primary key
            static const char* QUERY_JOB_BY_JOBID = "AzToolsFramework::AssetDatabase::QueryJobByJobID";
            static const char* QUERY_JOB_BY_JOBID_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "JobID = :jobid;";

            static const auto s_queryJobByJobid = MakeSqlQuery(QUERY_JOB_BY_JOBID, QUERY_JOB_BY_JOBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"));

            static const char* QUERY_JOB_BY_JOBKEY = "AzToolsFramework::AssetDatabase::QueryJobByJobKey";
            static const char* QUERY_JOB_BY_JOBKEY_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "JobKey = :jobKey;";

            static const auto s_queryJobByJobkey = MakeSqlQuery(QUERY_JOB_BY_JOBKEY, QUERY_JOB_BY_JOBKEY_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":jobKey"));

            static const char* QUERY_JOB_BY_JOBRUNKEY = "AzToolsFramework::AssetDatabase::QueryJobByJobRunKey";
            static const char* QUERY_JOB_BY_JOBRUNKEY_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "JobRunKey = :jobrunkey;";

            static const auto s_queryJobByJobrunkey = MakeSqlQuery(QUERY_JOB_BY_JOBRUNKEY, QUERY_JOB_BY_JOBRUNKEY_STATEMENT, LOG_NAME,
                    SqlParam<AZ::u64>(":jobrunkey"));

            static const char* QUERY_JOB_BY_PRODUCTID = "AzToolsFramework::AssetDatabase::QueryJobByProductID";
            static const char* QUERY_JOB_BY_PRODUCTID_STATEMENT =
                "SELECT Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.ProductID = :productid;";

            static const auto s_queryJobByProductid = MakeSqlQuery(QUERY_JOB_BY_PRODUCTID, QUERY_JOB_BY_PRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_JOB_BY_SOURCEID = "AzToolsFramework::AssetDatabase::QueryJobBySourceID";
            static const char* QUERY_JOB_BY_SOURCEID_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "SourcePK = :sourceid;";

            static const auto s_queryJobBySourceid = MakeSqlQuery(QUERY_JOB_BY_SOURCEID, QUERY_JOB_BY_SOURCEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"));

            static const char* QUERY_JOB_BY_SOURCEID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryJobBySourceIDPlatform";
            static const char* QUERY_JOB_BY_SOURCEID_PLATFORM_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "SourcePK = :sourceid AND "
                "Platform = :platform;";

            static const auto s_queryJobBySourceidPlatform = MakeSqlQuery(QUERY_JOB_BY_SOURCEID_PLATFORM, QUERY_JOB_BY_SOURCEID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"),
                    SqlParam<const char*>(":platform"));

            // lookup by primary key
            static const char* QUERY_PRODUCT_BY_PRODUCTID = "AzToolsFramework::AssetDatabase::QueryProductByProductID";
            static const char* QUERY_PRODUCT_BY_PRODUCTID_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.ProductID = :productid;";

            static const auto s_queryProductByProductid = MakeSqlQuery(QUERY_PRODUCT_BY_PRODUCTID, QUERY_PRODUCT_BY_PRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_PRODUCT_BY_JOBID = "AzToolsFramework::AssetDatabase::QueryProductByJobID";
            static const char* QUERY_PRODUCT_BY_JOBID_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.JobPK = :jobid;";

            static const auto s_queryProductByJobid = MakeSqlQuery(QUERY_PRODUCT_BY_JOBID, QUERY_PRODUCT_BY_JOBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"));

            static const char* QUERY_PRODUCT_BY_JOBID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductByJobIDPlatform";
            static const char* QUERY_PRODUCT_BY_JOBID_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.JobPK = :jobid AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryProductByJobidPlatform = MakeSqlQuery(QUERY_PRODUCT_BY_JOBID_PLATFORM, QUERY_PRODUCT_BY_JOBID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCT_BY_SOURCEID = "AzToolsFramework::AssetDatabase::QueryProductBySourceID";
            static const char* QUERY_PRODUCT_BY_SOURCEID_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.SourcePK = :sourceid;";

            static const auto s_queryProductBySourceid = MakeSqlQuery(QUERY_PRODUCT_BY_SOURCEID, QUERY_PRODUCT_BY_SOURCEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"));

            static const char* QUERY_PRODUCT_BY_SOURCEGUID_SUBID = "AzToolsFramework::AssetDatabase::QueryProductBySourceGuidSubid";
            static const char* QUERY_PRODUCT_BY_SOURCEGUID_SUBID_STATEMENT =
                "SELECT Sources.SourceGuid, Products.* FROM Sources INNER JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceGuid = :sourceguid AND Products.SubID = :productsubid;";

            static const auto s_queryProductBySourceGuidSubid = MakeSqlQuery(QUERY_PRODUCT_BY_SOURCEGUID_SUBID, QUERY_PRODUCT_BY_SOURCEGUID_SUBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::Uuid>(":sourceguid"),
                    SqlParam<AZ::u32>(":productsubid"));

            static const char* QUERY_PRODUCT_BY_SOURCEID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductBySourceIDPlatform";
            static const char* QUERY_PRODUCT_BY_SOURCEID_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.SourcePK = :sourceid AND "
                "Platform = :platform;";

            static const auto s_queryProductBySourceidPlatform = MakeSqlQuery(QUERY_PRODUCT_BY_SOURCEID_PLATFORM, QUERY_PRODUCT_BY_SOURCEID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCT_BY_PRODUCTNAME = "AzToolsFramework::AssetDatabase::QueryProductByProductName";
            static const char* QUERY_PRODUCT_BY_PRODUCTNAME_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.ProductName = :productname;";

            static const auto s_queryProductByProductname = MakeSqlQuery(QUERY_PRODUCT_BY_PRODUCTNAME, QUERY_PRODUCT_BY_PRODUCTNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"));

            static const char* QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductByProductNamePlatform";
            static const char* QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Products.ProductName = :productname;";

            static const auto s_queryProductByProductnamePlatform = MakeSqlQuery(QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM, QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCT_LIKE_PRODUCTNAME = "AzToolsFramework::AssetDatabase::QueryProductLikeProductName";
            static const char* QUERY_PRODUCT_LIKE_PRODUCTNAME_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.ProductName LIKE :productname ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryProductLikeProductname = MakeSqlQuery(QUERY_PRODUCT_LIKE_PRODUCTNAME, QUERY_PRODUCT_LIKE_PRODUCTNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"));

            static const char* QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductLikeProductNamePlatform";
            static const char* QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Products.ProductName LIKE :productname ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryProductLikeProductnamePlatform = MakeSqlQuery(QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM, QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCT_BY_SOURCENAME = "AzToolsFramework::AssetDatabase::QueryProductBySourceName";
            static const char* QUERY_PRODUCT_BY_SOURCENAME_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceName = :sourcename;";

            static const auto s_queryProductBySourcename = MakeSqlQuery(QUERY_PRODUCT_BY_SOURCENAME, QUERY_PRODUCT_BY_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            //add sql statement for querying everything by source name
            static const char* QUERY_PRODUCT_BY_SOURCENAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductBySourceNamePlatform";
            static const char* QUERY_PRODUCT_BY_SOURCENAME_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Sources.SourceName = :sourcename;";

            static const auto s_queryProductBySourcenamePlatform = MakeSqlQuery(QUERY_PRODUCT_BY_SOURCENAME_PLATFORM, QUERY_PRODUCT_BY_SOURCENAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by source name
            static const char* QUERY_PRODUCT_LIKE_SOURCENAME = "AzToolsFramework::AssetDatabase::QueryProductLikeSourceName";
            static const char* QUERY_PRODUCT_LIKE_SOURCENAME_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryProductLikeSourcename = MakeSqlQuery(QUERY_PRODUCT_LIKE_SOURCENAME, QUERY_PRODUCT_LIKE_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            //add sql statement for querying everything by source name and platform
            static const char* QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductLikeSourceNamePlatform";
            static const char* QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Sources.SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryProductLikeSourcenamePlatform = MakeSqlQuery(QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM, QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM_STATEMENT, LOG_NAME,
                SqlParam<const char*>(":sourcename"),
                SqlParam<const char*>(":platform"));

            // JobPK and subid together uniquely identify a product.  Since JobPK is indexed, this is a fast query if you happen to know those details.
            static const char* QUERY_PRODUCT_BY_JOBID_SUBID = "AzToolsFramework::AssetDatabase::QueryProductByJobIDSubID";
            static const char* QUERY_PRODUCT_BY_JOBID_SUBID_STATEMENT =
                "SELECT * FROM Products "
                "WHERE JobPK = :jobpk "
                "AND SubID = :subid;";

            static const auto s_queryProductByJobIdSubId = MakeSqlQuery(QUERY_PRODUCT_BY_JOBID_SUBID, QUERY_PRODUCT_BY_JOBID_SUBID_STATEMENT, LOG_NAME,
                SqlParam<AZ::s64>(":jobpk"),
                SqlParam<AZ::u32>(":subid"));

            //add sql statement for querying everything by platform
            static const char* QUERY_COMBINED = "AzToolsFramework::AssetDatabase::QueryCombined";
            static const char* QUERY_COMBINED_STATEMENT =
                "SELECT * FROM ScanFolders INNER JOIN Sources "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK;";

            static const auto s_queryCombined = MakeSqlQuery(QUERY_COMBINED, QUERY_COMBINED_STATEMENT, LOG_NAME);

            //add sql statement for querying everything by platform
            static const char* QUERY_COMBINED_BY_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedByPlatform";
            static const char* QUERY_COMBINED_BY_PLATFORM_STATEMENT =
                "SELECT * FROM Jobs LEFT JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedByPlatform = MakeSqlQuery(QUERY_COMBINED_BY_PLATFORM, QUERY_COMBINED_BY_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_COMBINED_BY_SOURCEID = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceID";
            static const char* QUERY_COMBINED_BY_SOURCEID_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceID = :sourceid;";

            static const auto s_queryCombinedBySourceid = MakeSqlQuery(QUERY_COMBINED_BY_SOURCEID, QUERY_COMBINED_BY_SOURCEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"));

            static const char* QUERY_COMBINED_BY_SOURCEID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceIDPlatform";
            static const char* QUERY_COMBINED_BY_SOURCEID_PLATFORM_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceID = :sourceid AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedBySourceidPlatform = MakeSqlQuery(QUERY_COMBINED_BY_SOURCEID_PLATFORM, QUERY_COMBINED_BY_SOURCEID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_COMBINED_BY_JOBID = "AzToolsFramework::AssetDatabase::QueryCombinedByJobID";
            static const char* QUERY_COMBINED_BY_JOBID_STATEMENT =
                "SELECT * FROM Jobs LEFT JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.JobID = :jobid;";

            static const auto s_queryCombinedByJobid = MakeSqlQuery(QUERY_COMBINED_BY_JOBID, QUERY_COMBINED_BY_JOBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"));

            static const char* QUERY_COMBINED_BY_JOBID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedByJobIDPlatform";
            static const char* QUERY_COMBINED_BY_JOBID_PLATFORM_STATEMENT =
                "SELECT * FROM Jobs LEFT JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.JobID = :jobid AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedByJobidPlatform = MakeSqlQuery(QUERY_COMBINED_BY_JOBID_PLATFORM, QUERY_COMBINED_BY_JOBID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_COMBINED_BY_PRODUCTID = "AzToolsFramework::AssetDatabase::QueryCombinedByProcductID";
            static const char* QUERY_COMBINED_BY_PRODUCTID_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Products.ProductID = :productid;";

            static const auto s_queryCombinedByProductid = MakeSqlQuery(QUERY_COMBINED_BY_PRODUCTID, QUERY_COMBINED_BY_PRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_COMBINED_BY_PRODUCTID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedByProductIDPlatform";
            static const char* QUERY_COMBINED_BY_PRODUCTID_PLATFORM_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Products.ProductID = :productid AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedByProductidPlatform = MakeSqlQuery(QUERY_COMBINED_BY_PRODUCTID_PLATFORM, QUERY_COMBINED_BY_PRODUCTID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"),
                    SqlParam<const char*>(":platform"));


            //add sql statement for querying everything by source guid and product subid
            static const char* QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceGuidProductSubID";
            static const char* QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Products.SubID = :productsubid AND "
                "(Sources.SourceGuid = :sourceguid OR "
                "Products.LegacyGuid = :sourceguid);";

            static const auto s_queryCombinedBySourceguidProductsubid = MakeSqlQuery(QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID, QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::u32>(":productsubid"),
                    SqlParam<AZ::Uuid>(":sourceguid"));

            //add sql statement for querying everything by source guid and product subid and platform
            static const char* QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceGuidProductSubIDPlatform";
            static const char* QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK  WHERE "
                "Products.SubID = :productsubid AND "
                "(Sources.SourceGuid = :sourceguid OR "
                "Products.LegacyGuid = :soruceguid) AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedBySourceguidProductsubidPlatform = MakeSqlQuery(QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM, QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::u32>(":productsubid"),
                    SqlParam<AZ::Uuid>(":sourceguid"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by source name
            static const char* QUERY_COMBINED_BY_SOURCENAME = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceName";
            static const char* QUERY_COMBINED_BY_SOURCENAME_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceName = :sourcename;";

            static const auto s_queryCombinedBySourcename = MakeSqlQuery(QUERY_COMBINED_BY_SOURCENAME, QUERY_COMBINED_BY_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            //add sql statement for querying everything by source name
            static const char* QUERY_COMBINED_BY_SOURCENAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceNamePlatform";
            static const char* QUERY_COMBINED_BY_SOURCENAME_PLATFORM_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Sources.SourceName = :sourcename;";

            static const auto s_queryCombinedBySourcenamePlatform = MakeSqlQuery(QUERY_COMBINED_BY_SOURCENAME_PLATFORM, QUERY_COMBINED_BY_SOURCENAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by source name
            static const char* QUERY_COMBINED_LIKE_SOURCENAME = "AzToolsFramework::AssetDatabase::QueryCombinedLikeSourceName";
            static const char* QUERY_COMBINED_LIKE_SOURCENAME_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Sources.SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryCombinedLikeSourcename = MakeSqlQuery(QUERY_COMBINED_LIKE_SOURCENAME, QUERY_COMBINED_LIKE_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            //add sql statement for querying everything by source name and platform
            static const char* QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedLikeSourceNamePlatform";
            static const char* QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Sources.SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryCombinedLikeSourcenamePlatform = MakeSqlQuery(QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM, QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by product name
            static const char* QUERY_COMBINED_BY_PRODUCTNAME = "AzToolsFramework::AssetDatabase::QueryCombinedByProductName";
            static const char* QUERY_COMBINED_BY_PRODUCTNAME_STATEMENT =
                "SELECT * "
                "FROM Products "
                "LEFT OUTER JOIN Jobs ON Jobs.JobID = Products.JobPK "
                "LEFT OUTER JOIN Sources ON Sources.SourceID = Jobs.SourcePK "
                "LEFT OUTER JOIN ScanFolders ON ScanFolders.ScanFolderID = Sources.SourceID "
                "WHERE "
                "Products.ProductName = :productname;";

            static const auto s_queryCombinedByProductname = MakeSqlQuery(QUERY_COMBINED_BY_PRODUCTNAME, QUERY_COMBINED_BY_PRODUCTNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"));

            //add sql statement for querying everything by product name and platform
            static const char* QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedByProductNamePlatorm";
            static const char* QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM_STATEMENT =
                "SELECT * "
                "FROM Products "
                "LEFT OUTER JOIN Jobs ON Jobs.JobID = Products.JobPK "
                "LEFT OUTER JOIN Sources ON Sources.SourceID = Jobs.SourcePK "
                "LEFT OUTER JOIN ScanFolders ON ScanFolders.ScanFolderID = Sources.SourceID "
                "WHERE "
                "Products.ProductName = :productname AND"
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedByProductnamePlatform = MakeSqlQuery(QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM, QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by product name
            static const char* QUERY_COMBINED_LIKE_PRODUCTNAME = "AzToolsFramework::AssetDatabase::QueryCombinedLikeProductName";
            static const char* QUERY_COMBINED_LIKE_PRODUCTNAME_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Products.ProductName LIKE :productname ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryCombinedLikeProductname = MakeSqlQuery(QUERY_COMBINED_LIKE_PRODUCTNAME, QUERY_COMBINED_LIKE_PRODUCTNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"));

            //add sql statement for querying everything by product name and platform
            static const char* QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedLikeProductNamePlatorm";
            static const char* QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Jobs.Platform = :platform AND "
                "Products.ProductName LIKE :productname ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryCombinedLikeProductnamePlatform = MakeSqlQuery(QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM, QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID = "AzToolsFramework::AssetDatabase::QuerySourceDependencyBySourceDependencyID";
            static const char* QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID_STATEMENT =
                "SELECT * FROM SourceDependency WHERE "
                "SourceDependencyID = :sourceDependencyid;";

            static const auto s_querySourcedependencyBySourcedependencyid = MakeSqlQuery(QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID, QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceDependencyid"));

            static const char* QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE = "AzToolsFramework::AssetDatabase::QuerySourceDependencyByDependsOnSource";
            static const char* QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE_STATEMENT =
                "SELECT * from SourceDependency WHERE "
                "DependsOnSource = :dependsOnSource AND "
                "TypeOfDependency & :typeOfDependency AND "
                "Source LIKE :dependentFilter;";

            static const auto s_querySourcedependencyByDependsonsource = MakeSqlQuery(QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE, QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE_STATEMENT, LOG_NAME,
                SqlParam<const char*>(":dependsOnSource"),
                SqlParam<const char*>(":dependentFilter"),
                SqlParam<AZ::u32>(":typeOfDependency"));

            static const char* QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE_WILDCARD = "AzToolsFramework::AssetDatabase::QuerySourceDependencyByDependsOnSourceWildcard";
            static const char* QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE_WILDCARD_STATEMENT =
                "SELECT * from SourceDependency WHERE "
                "((TypeOfDependency & :typeOfDependency AND "
                "DependsOnSource = :dependsOnSource) OR "
                "(TypeOfDependency = :wildCardDependency AND "
                ":dependsOnSource LIKE DependsOnSource)) AND "
                "Source LIKE :dependentFilter;";

            static const auto s_querySourcedependencyByDependsonsourceWildcard = MakeSqlQuery(QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE_WILDCARD, QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE_WILDCARD_STATEMENT, LOG_NAME,
                SqlParam<const char*>(":dependsOnSource"),
                SqlParam<const char*>(":dependentFilter"),
                SqlParam<AZ::u32>(":typeOfDependency"),
                SqlParam<AZ::u32>(":wildCardDependency"));

            static const char* QUERY_DEPENDSONSOURCE_BY_SOURCE = "AzToolsFramework::AssetDatabase::QueryDependsOnSourceBySource";
            static const char* QUERY_DEPENDSONSOURCE_BY_SOURCE_STATEMENT =
                "SELECT * from SourceDependency WHERE "
                "Source = :source AND "
                "TypeOfDependency & :typeOfDependency AND "
                "DependsOnSource LIKE :dependencyFilter;";
            static const auto s_queryDependsonsourceBySource = MakeSqlQuery(QUERY_DEPENDSONSOURCE_BY_SOURCE, QUERY_DEPENDSONSOURCE_BY_SOURCE_STATEMENT, LOG_NAME,
                SqlParam<const char*>(":source"),
                SqlParam<const char*>(":dependencyFilter"),
                SqlParam<AZ::u32>(":typeOfDependency"));

            static const char* QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID = "AzToolsFramework::AssetDatabase::QueryProductDependencyByProductDependencyID";
            static const char* QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID_STATEMENT =
                "SELECT * FROM ProductDependencies WHERE "
                "ProductDependencyID = :productdependencyid;";

            static const auto s_queryProductdependencyByProductdependencyid = MakeSqlQuery(QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID, QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productdependencyid"));

            static const char* QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID = "AzToolsFramework::AssetDatabase::QueryProductDependencyByProductID";
            static const char* QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID_STATEMENT =
                "SELECT * FROM ProductDependencies WHERE "
                "ProductPK = :productid;";

            static const auto s_queryProductdependencyByProductid = MakeSqlQuery(QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID, QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_DIRECT_PRODUCTDEPENDENCIES = "AzToolsFramework::AssetDatabase::QueryDirectProductDependencies";
            static const char* QUERY_DIRECT_PRODUCTDEPENDENCIES_STATEMENT =
                "SELECT * FROM Products "
                "LEFT OUTER JOIN Jobs ON Jobs.JobID = Products.JobPK "
                "LEFT OUTER JOIN Sources ON Sources.SourceID = Jobs.SourcePK "
                "LEFT OUTER JOIN ProductDependencies "
                "  ON Sources.SourceGuid = ProductDependencies.DependencySourceGuid "
                "  AND Products.SubID = ProductDependencies.DependencySubID "
                "WHERE ProductDependencies.ProductPK = :productid;";

            static const auto s_queryDirectProductdependencies = MakeSqlQuery(QUERY_DIRECT_PRODUCTDEPENDENCIES, QUERY_DIRECT_PRODUCTDEPENDENCIES_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_ALL_PRODUCTDEPENDENCIES = "AzToolsFramework::AssetDatabase::QueryAllProductDependencies";
            static const char* QUERY_ALL_PRODUCTDEPENDENCIES_STATEMENT =
                "WITH RECURSIVE "
                "  allProductDeps(ProductID, JobPK, ProductName, SubID, AssetType, LegacyGuid) AS (  "
                "    SELECT * FROM Products "
                "    WHERE ProductID = :productid "
                "    UNION "
                "    SELECT P.ProductID, P.JobPK, P.ProductName, P.SubID, P.AssetType, P.LegacyGuid FROM Products P, allProductDeps"
                "    LEFT OUTER JOIN Jobs ON Jobs.JobID = P.JobPK "
                "    LEFT OUTER JOIN Sources ON Sources.SourceID = Jobs.SourcePK "
                "    LEFT OUTER JOIN ProductDependencies"
                "    ON Sources.SourceGuid = ProductDependencies.DependencySourceGuid "
                "    AND P.SubID = ProductDependencies.DependencySubID "
                "    WHERE ProductDependencies.ProductPK = allProductDeps.ProductID "
                "    LIMIT -1 OFFSET 1 " // This will ignore the first Product selected which is not a dependency but the root of the tree
                "  ) "
                "SELECT * FROM allProductDeps;";

            static const auto s_queryAllProductdependencies = MakeSqlQuery(QUERY_ALL_PRODUCTDEPENDENCIES, QUERY_ALL_PRODUCTDEPENDENCIES_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* GET_UNRESOLVED_PRODUCT_DEPENDENCIES = "AssetProcessor::GetUnresolvedProductDependencies";
            static const char* GET_UNRESOLVED_PRODUCT_DEPENDENCIES_STATEMENT =
                "SELECT * FROM ProductDependencies where UnresolvedPath != ''";
            static const auto s_queryUnresolvedProductDependencies = MakeSqlQuery(GET_UNRESOLVED_PRODUCT_DEPENDENCIES, GET_UNRESOLVED_PRODUCT_DEPENDENCIES_STATEMENT, LOG_NAME);


            // lookup by primary key
            static const char* QUERY_FILE_BY_FILEID = "AzToolsFramework::AssetDatabase::QueryFileByFileID";
            static const char* QUERY_FILE_BY_FILEID_STATEMENT =
                "SELECT * FROM Files WHERE "
                "FileID = :fileid;";

            static const auto s_queryFileByFileid = MakeSqlQuery(QUERY_FILE_BY_FILEID, QUERY_FILE_BY_FILEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":fileid"));

            static const char* QUERY_FILES_BY_FILENAME_AND_SCANFOLDER = "AzToolsFramework::AssetDatabase::QueryFilesByFileNameAndScanFolderID";
            static const char* QUERY_FILES_BY_FILENAME_AND_SCANFOLDER_STATEMENT =
                "SELECT * FROM Files WHERE "
                    "ScanFolderPK = :scanfolderpk AND "
                    "FileName = :filename;";

            static const auto s_queryFilesByFileName = MakeSqlQuery(QUERY_FILES_BY_FILENAME_AND_SCANFOLDER, QUERY_FILES_BY_FILENAME_AND_SCANFOLDER_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":scanfolderpk"),
                    SqlParam<const char*>(":filename") 
                );

            static const char* QUERY_FILES_LIKE_FILENAME = "AzToolsFramework::AssetDatabase::QueryFilesLikeFileName";
            static const char* QUERY_FILES_LIKE_FILENAME_STATEMENT =
                "SELECT * FROM Files WHERE "
                "FileName LIKE :filename ESCAPE '|';";

            static const auto s_queryFilesLikeFileName = MakeSqlQuery(QUERY_FILES_LIKE_FILENAME, QUERY_FILES_LIKE_FILENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":filename"));

            static const char* QUERY_FILES_BY_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QueryFilesByScanFolderID";
            static const char* QUERY_FILES_BY_SCANFOLDERID_STATEMENT =
                "SELECT * FROM Files WHERE "
                "ScanFolderPK = :scanfolderid;";

            static const auto s_queryFilesByScanfolderid = MakeSqlQuery(QUERY_FILES_BY_SCANFOLDERID, QUERY_FILES_BY_SCANFOLDERID_STATEMENT, LOG_NAME,
                SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_FILE_BY_FILENAME_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QueryFileByFileNameScanFolderID";
            static const char* QUERY_FILE_BY_FILENAME_SCANFOLDERID_STATEMENT =
                "SELECT * FROM Files WHERE "
                "ScanFolderPK = :scanfolderid AND "
                "FileName = :filename;";

            static const auto s_queryFileByFileNameScanfolderid = MakeSqlQuery(QUERY_FILE_BY_FILENAME_SCANFOLDERID, QUERY_FILE_BY_FILENAME_SCANFOLDERID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":scanfolderid"),
                    SqlParam<const char*>(":filename"));

            void PopulateJobInfo(AzToolsFramework::AssetSystem::JobInfo& jobinfo, JobDatabaseEntry& jobDatabaseEntry)
            {
                jobinfo.m_platform = AZStd::move(jobDatabaseEntry.m_platform);
                jobinfo.m_builderGuid = jobDatabaseEntry.m_builderGuid;
                jobinfo.m_jobKey = AZStd::move(jobDatabaseEntry.m_jobKey);
                jobinfo.m_status = jobDatabaseEntry.m_status;
                jobinfo.m_jobRunKey = jobDatabaseEntry.m_jobRunKey;
                jobinfo.m_firstFailLogTime = jobDatabaseEntry.m_firstFailLogTime;
                jobinfo.m_firstFailLogFile = AZStd::move(jobDatabaseEntry.m_firstFailLogFile);
                jobinfo.m_lastFailLogTime = jobDatabaseEntry.m_lastFailLogTime;
                jobinfo.m_lastFailLogFile = AZStd::move(jobDatabaseEntry.m_lastFailLogFile);
                jobinfo.m_lastLogTime = jobDatabaseEntry.m_lastLogTime;
                jobinfo.m_lastLogFile = AZStd::move(jobDatabaseEntry.m_lastLogFile);
                jobinfo.m_jobID = jobDatabaseEntry.m_jobID;
            }
        }
        
        //////////////////////////////////////////////////////////////////////////
        //DatabaseInfoEntry
        DatabaseInfoEntry::DatabaseInfoEntry(AZ::s64 rowID, DatabaseVersion version)
            : m_rowID(rowID)
            , m_version(version)
        {
        }

        auto DatabaseInfoEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("rowID", m_rowID),
                MakeColumn("version", m_version)
            );
        }

        //////////////////////////////////////////////////////////////////////////
        //ScanFolderDatabaseEntry
        ScanFolderDatabaseEntry::ScanFolderDatabaseEntry(
            AZ::s64 scanFolderID,
            const char* scanFolder,
            const char* displayName,
            const char* portableKey,
            const char* outputPrefix,
            int isRoot)
            : m_scanFolderID(scanFolderID)
            , m_outputPrefix(outputPrefix)
            , m_isRoot(isRoot)
        {
            if (scanFolder)
            {
                m_scanFolder = scanFolder;
            }
            if (displayName)
            {
                m_displayName = displayName;
            }
            if (portableKey)
            {
                m_portableKey = portableKey;
            }
        }

        ScanFolderDatabaseEntry::ScanFolderDatabaseEntry(
            const char* scanFolder,
            const char* displayName,
            const char* portableKey,
            const char* outputPrefix,
            int isRoot)
            : m_scanFolderID(-1)
            , m_outputPrefix(outputPrefix)
            , m_isRoot(isRoot)
        {
            if (scanFolder)
            {
                m_scanFolder = scanFolder;
            }

            if (displayName)
            {
                m_displayName = displayName;
            }

            if (portableKey)
            {
                m_portableKey = portableKey;
            }
        }

        ScanFolderDatabaseEntry::ScanFolderDatabaseEntry(const ScanFolderDatabaseEntry& other)
            : m_scanFolderID(other.m_scanFolderID)
            , m_scanFolder(other.m_scanFolder)
            , m_displayName(other.m_displayName)
            , m_portableKey(other.m_portableKey)
            , m_outputPrefix(other.m_outputPrefix)
            , m_isRoot(other.m_isRoot)
        {
        }

        ScanFolderDatabaseEntry::ScanFolderDatabaseEntry(ScanFolderDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        ScanFolderDatabaseEntry& ScanFolderDatabaseEntry::operator=(ScanFolderDatabaseEntry&& other)
        {
            if (this != &other)
            {
                m_scanFolder = AZStd::move(other.m_scanFolder);
                m_scanFolderID = other.m_scanFolderID;
                m_displayName = AZStd::move(other.m_displayName);
                m_portableKey = AZStd::move(other.m_portableKey);
                m_outputPrefix = AZStd::move(other.m_outputPrefix);
                m_isRoot = other.m_isRoot;
            }
            return *this;
        }

        ScanFolderDatabaseEntry& ScanFolderDatabaseEntry::operator=(const ScanFolderDatabaseEntry& other)
        {
            m_scanFolder = other.m_scanFolder;
            m_scanFolderID = other.m_scanFolderID;
            m_displayName = other.m_displayName;
            m_portableKey = other.m_portableKey;
            m_outputPrefix = other.m_outputPrefix;
            m_isRoot = other.m_isRoot;
            return *this;
        }

        bool ScanFolderDatabaseEntry::operator==(const ScanFolderDatabaseEntry& other) const
        {
            // its the same database entry when the portable key is the same.
            return m_portableKey == other.m_portableKey;
        }

        AZStd::string ScanFolderDatabaseEntry::ToString() const
        {
            return AZStd::string::format("ScanFolderDatabaseEntry id:%i path: %s, displayname: %s, portable key: %s",
                m_scanFolderID,
                m_scanFolder.c_str(),
                m_displayName.c_str(),
                m_portableKey.c_str());
        }

        auto ScanFolderDatabaseEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("ScanFolderID", m_scanFolderID),
                MakeColumn("ScanFolder", m_scanFolder),
                MakeColumn("DisplayName", m_displayName),
                MakeColumn("PortableKey", m_portableKey),
                MakeColumn("OutputPrefix", m_outputPrefix),
                MakeColumn("IsRoot", m_isRoot)
            );
        }

        //////////////////////////////////////////////////////////////////////////
        //SourceDatabaseEntry
        SourceDatabaseEntry::SourceDatabaseEntry(AZ::s64 sourceID, AZ::s64 scanFolderPK, const char* sourceName, AZ::Uuid sourceGuid, const char* analysisFingerprint)
            : m_sourceID(sourceID)
            , m_scanFolderPK(scanFolderPK)
            , m_sourceGuid(sourceGuid)
        {
            if (sourceName)
            {
                m_sourceName = sourceName;
            }
            if (analysisFingerprint)
            {
                m_analysisFingerprint = analysisFingerprint;
            }
        }

        SourceDatabaseEntry::SourceDatabaseEntry(AZ::s64 scanFolderPK, const char* sourceName, AZ::Uuid sourceGuid, const char* analysisFingerprint)
            : m_sourceID(-1)
            , m_scanFolderPK(scanFolderPK)
            , m_sourceGuid(sourceGuid)
        {
            if (sourceName)
            {
                m_sourceName = sourceName;
            }

            if (analysisFingerprint)
            {
                m_analysisFingerprint = analysisFingerprint;
            }
        }

        AZStd::string SourceDatabaseEntry::ToString() const
        {
            return AZStd::string::format("SourceDatabaseEntry id:%i scanfolderpk: %i sourcename: %s sourceguid: %s", m_sourceID, m_scanFolderPK, m_sourceName.c_str(), m_sourceGuid.ToString<AZStd::string>().c_str());
        }

        auto SourceDatabaseEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("SourceID", m_sourceID),
                MakeColumn("ScanFolderPK", m_scanFolderPK),
                MakeColumn("SourceName", m_sourceName),
                MakeColumn("SourceGuid", m_sourceGuid),
                MakeColumn("AnalysisFingerprint", m_analysisFingerprint)
            );
        }

        //////////////////////////////////////////////////////////////////////////
        // BuilderInfoEntry
        BuilderInfoEntry::BuilderInfoEntry(AZ::s64 builderInfoID, const AZ::Uuid& builderUuid, const char* analysisFingerprint)
            : m_builderInfoID(builderInfoID)
            , m_builderUuid(builderUuid)
        {
            if (analysisFingerprint)
            {
                m_analysisFingerprint = analysisFingerprint;
            }
        }

        AZStd::string BuilderInfoEntry::ToString() const
        {
            return AZStd::string::format("BuilderInfoEntry id:%i uuid: %s fingerprint: %s", m_builderInfoID, m_builderUuid.ToString<AZStd::string>().c_str(), m_analysisFingerprint.c_str());
        }

        auto BuilderInfoEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("BuilderID", m_builderInfoID),
                MakeColumn("Guid", m_builderUuid),
                MakeColumn("AnalysisFingerprint", m_analysisFingerprint)
            );
        }

        //////////////////////////////////////////////////////////////////////////
        //SourceFileDependencyEntry

        SourceFileDependencyEntry::SourceFileDependencyEntry(AZ::Uuid builderGuid, const char *source, const char* dependsOnSource, SourceFileDependencyEntry::TypeOfDependency dependencyType)
            : m_builderGuid(builderGuid)
            , m_source(source)
            , m_dependsOnSource(dependsOnSource)
            , m_typeOfDependency(dependencyType)
        {
            AZ_Assert(dependencyType != SourceFileDependencyEntry::DEP_Any, "You may only store actual dependency types in the database, not DEP_Any");
        }

        AZStd::string SourceFileDependencyEntry::ToString() const
        {
            return AZStd::string::format("SourceFileDependencyEntry id:%i builderGuid: %s source: %s dependsOnSource: %s type: %s", m_sourceDependencyID, m_builderGuid.ToString<AZStd::string>().c_str(), m_source.c_str(), m_dependsOnSource.c_str(), m_typeOfDependency == DEP_SourceToSource ? "source" : "job");
        }

        auto SourceFileDependencyEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("SourceDependencyID", m_sourceDependencyID),
                MakeColumn("BuilderGuid", m_builderGuid),
                MakeColumn("Source", m_source),
                MakeColumn("DependsOnSource", m_dependsOnSource),
                MakeColumn("TypeOfDependency", m_typeOfDependency)
            );
        }

        //////////////////////////////////////////////////////////////////////////
        //JobDatabaseEntry
        JobDatabaseEntry::JobDatabaseEntry(AZ::s64 jobID, AZ::s64 sourcePK, const char* jobKey, AZ::u32 fingerprint, const char* platform, AZ::Uuid builderGuid, AssetSystem::JobStatus status, AZ::u64 jobRunKey, AZ::s64 firstFailLogTime, const char* firstFailLogFile, AZ::s64 lastFailLogTime, const char* lastFailLogFile, AZ::s64 lastLogTime, const char* lastLogFile)
            : m_jobID(jobID)
            , m_sourcePK(sourcePK)
            , m_fingerprint(fingerprint)
            , m_builderGuid(builderGuid)
            , m_status(status)
            , m_jobRunKey(jobRunKey)
            , m_firstFailLogTime(firstFailLogTime)
            , m_lastFailLogTime(lastFailLogTime)
            , m_lastLogTime(lastLogTime)
        {
            if (jobKey)
            {
                m_jobKey = jobKey;
            }
            if (platform)
            {
                m_platform = platform;
            }
            if (firstFailLogFile)
            {
                m_firstFailLogFile = firstFailLogFile;
            }
            if (lastFailLogFile)
            {
                m_lastFailLogFile = lastFailLogFile;
            }
            if (lastLogFile)
            {
                m_lastLogFile = lastLogFile;
            }
        }

        JobDatabaseEntry::JobDatabaseEntry(AZ::s64 sourcePK, const char* jobKey, AZ::u32 fingerprint, const char* platform, AZ::Uuid builderGuid, AssetSystem::JobStatus status, AZ::u64 jobRunKey, AZ::s64 firstFailLogTime, const char* firstFailLogFile, AZ::s64 lastFailLogTime, const char* lastFailLogFile, AZ::s64 lastLogTime, const char* lastLogFile)
            : m_jobID(-1)
            , m_sourcePK(sourcePK)
            , m_fingerprint(fingerprint)
            , m_builderGuid(builderGuid)
            , m_status(status)
            , m_jobRunKey(jobRunKey)
            , m_firstFailLogTime(firstFailLogTime)
            , m_lastFailLogTime(lastFailLogTime)
            , m_lastLogTime(lastLogTime)
        {
            if (jobKey)
            {
                m_jobKey = jobKey;
            }
            if (platform)
            {
                m_platform = platform;
            }
            if (firstFailLogFile)
            {
                m_firstFailLogFile = firstFailLogFile;
            }
            if (lastFailLogFile)
            {
                m_lastFailLogFile = lastFailLogFile;
            }
            if (lastLogFile)
            {
                m_lastLogFile = lastLogFile;
            }
        }

        JobDatabaseEntry::JobDatabaseEntry(const JobDatabaseEntry& other)
            : m_jobID(other.m_jobID)
            , m_sourcePK(other.m_sourcePK)
            , m_jobKey(other.m_jobKey)
            , m_fingerprint(other.m_fingerprint)
            , m_platform(other.m_platform)
            , m_builderGuid(other.m_builderGuid)
            , m_status(other.m_status)
            , m_jobRunKey(other.m_jobRunKey)
            , m_firstFailLogTime(other.m_firstFailLogTime)
            , m_firstFailLogFile(other.m_firstFailLogFile)
            , m_lastFailLogTime(other.m_lastFailLogTime)
            , m_lastFailLogFile(other.m_lastFailLogFile)
            , m_lastLogTime(other.m_lastLogTime)
            , m_lastLogFile(other.m_lastLogFile)
        {
        }

        JobDatabaseEntry::JobDatabaseEntry(JobDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        JobDatabaseEntry& JobDatabaseEntry::operator=(JobDatabaseEntry&& other)
        {
            if (this != &other)
            {
                m_jobID = other.m_jobID;
                m_sourcePK = other.m_sourcePK;
                m_jobKey = AZStd::move(other.m_jobKey);
                m_fingerprint = other.m_fingerprint;
                m_platform = AZStd::move(other.m_platform);
                m_builderGuid = other.m_builderGuid;
                m_status = other.m_status;
                m_jobRunKey = other.m_jobRunKey;
                m_firstFailLogTime = other.m_firstFailLogTime;
                m_firstFailLogFile = AZStd::move(other.m_firstFailLogFile);
                m_lastFailLogTime = other.m_lastFailLogTime;
                m_lastFailLogFile = AZStd::move(other.m_lastFailLogFile);
                m_lastLogTime = other.m_lastLogTime;
                m_lastLogFile = AZStd::move(other.m_lastLogFile);
            }
            return *this;
        }

        JobDatabaseEntry& JobDatabaseEntry::operator=(const JobDatabaseEntry& other)
        {
            m_jobID = other.m_jobID;
            m_sourcePK = other.m_sourcePK;
            m_jobKey = other.m_jobKey;
            m_fingerprint = other.m_fingerprint;
            m_platform = other.m_platform;
            m_builderGuid = other.m_builderGuid;
            m_status = other.m_status;
            m_jobRunKey = other.m_jobRunKey;
            m_firstFailLogTime = other.m_firstFailLogTime;
            m_firstFailLogFile = other.m_firstFailLogFile;
            m_lastFailLogTime = other.m_lastFailLogTime;
            m_lastFailLogFile = other.m_lastFailLogFile;
            m_lastLogTime = other.m_lastLogTime;
            m_lastLogFile = other.m_lastLogFile;

            return *this;
        }

        bool JobDatabaseEntry::operator==(const JobDatabaseEntry& other) const
        {
            //equivalence is when everything but the id is the same
            return m_sourcePK == other.m_sourcePK &&
                   m_fingerprint == other.m_fingerprint &&
                   AzFramework::StringFunc::Equal(m_jobKey.c_str(), other.m_jobKey.c_str()) &&
                   AzFramework::StringFunc::Equal(m_platform.c_str(), other.m_platform.c_str()) &&
                   m_builderGuid == other.m_builderGuid &&
                   m_status == other.m_status &&
                   m_jobRunKey == other.m_jobRunKey &&
                   m_firstFailLogTime == other.m_firstFailLogTime &&
                   AzFramework::StringFunc::Equal(m_firstFailLogFile.c_str(), other.m_firstFailLogFile.c_str()) &&
                   m_lastFailLogTime == other.m_lastFailLogTime &&
                   AzFramework::StringFunc::Equal(m_lastFailLogFile.c_str(), other.m_lastFailLogFile.c_str()) &&
                   m_lastLogTime == other.m_lastLogTime &&
                   AzFramework::StringFunc::Equal(m_lastLogFile.c_str(), other.m_lastLogFile.c_str());
        }

        AZStd::string JobDatabaseEntry::ToString() const
        {
            return AZStd::string::format("JobDatabaseEntry id:%i sourcepk: %i jobkey: %s fingerprint: %i platform: %s builderguid: %s status: %s", m_jobID, m_sourcePK, m_jobKey.c_str(), m_fingerprint, m_platform.c_str(), m_builderGuid.ToString<AZStd::string>().c_str(), AssetSystem::JobStatusString(m_status));
        }

        auto JobDatabaseEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("JobID", m_jobID),
                MakeColumn("SourcePK", m_sourcePK),
                MakeColumn("JobKey", m_jobKey),
                MakeColumn("Fingerprint", m_fingerprint),
                MakeColumn("Platform", m_platform),
                MakeColumn("BuilderGuid", m_builderGuid),
                MakeColumn("Status", m_status),
                MakeColumn("JobRunKey", m_jobRunKey),
                MakeColumn("FirstFailLogTime", m_firstFailLogTime),
                MakeColumn("FirstFailLogFile", m_firstFailLogFile),
                MakeColumn("LastFailLogTime", m_lastFailLogTime),
                MakeColumn("LastFailLogFile", m_lastFailLogFile),
                MakeColumn("LastLogTime", m_lastLogTime),
                MakeColumn("LastLogFile", m_lastLogFile)
            );
        }

        //////////////////////////////////////////////////////////////////////////
        //ProductDatabaseEntry
        ProductDatabaseEntry::ProductDatabaseEntry(AZ::s64 productID, AZ::s64 jobPK, AZ::u32 subID, const char* productName,
            AZ::Data::AssetType assetType, AZ::Uuid legacyGuid)
            : m_productID(productID)
            , m_jobPK(jobPK)
            , m_subID(subID)
            , m_assetType(assetType)
            , m_legacyGuid(legacyGuid)
        {
            if (productName)
            {
                m_productName = productName;
            }
        }

        ProductDatabaseEntry::ProductDatabaseEntry(AZ::s64 jobPK, AZ::u32 subID, const char* productName,
            AZ::Data::AssetType assetType, AZ::Uuid legacyGuid)
            : m_productID(-1)
            , m_jobPK(jobPK)
            , m_subID(subID)
            , m_assetType(assetType)
            , m_legacyGuid(legacyGuid)
        {
            if (productName)
            {
                m_productName = productName;
            }
        }

        ProductDatabaseEntry::ProductDatabaseEntry(const ProductDatabaseEntry& other)
            : m_productID(other.m_productID)
            , m_jobPK(other.m_jobPK)
            , m_subID(other.m_subID)
            , m_productName(other.m_productName)
            , m_assetType(other.m_assetType)
            , m_legacyGuid(other.m_legacyGuid)
        {
        }

        ProductDatabaseEntry::ProductDatabaseEntry(ProductDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        ProductDatabaseEntry& ProductDatabaseEntry::operator=(ProductDatabaseEntry&& other)
        {
            if (this != &other)
            {
                m_productID = other.m_productID;
                m_jobPK = other.m_jobPK;
                m_subID = other.m_subID;
                m_productName = AZStd::move(other.m_productName);
                m_assetType = other.m_assetType;
                m_legacyGuid = other.m_legacyGuid;
            }
            return *this;
        }

        ProductDatabaseEntry& ProductDatabaseEntry::operator=(const ProductDatabaseEntry& other)
        {
            m_productID = other.m_productID;
            m_jobPK = other.m_jobPK;
            m_subID = other.m_subID;
            m_productName = other.m_productName;
            m_assetType = other.m_assetType;
            m_legacyGuid = other.m_legacyGuid;
            return *this;
        }

        bool ProductDatabaseEntry::operator==(const ProductDatabaseEntry& other) const
        {
            //equivalence is when everything but the id is the same
            return m_jobPK == other.m_jobPK &&
                   m_subID == other.m_subID &&
                   m_assetType == other.m_assetType &&
                   AzFramework::StringFunc::Equal(m_productName.c_str(), other.m_productName.c_str());//don't compare legacy guid
        }

        AZStd::string ProductDatabaseEntry::ToString() const
        {
            return AZStd::string::format("ProductDatabaseEntry id:%i jobpk: %i subid: %i productname: %s assettype: %s", m_productID, m_jobPK, m_subID, m_productName.c_str(), m_assetType.ToString<AZStd::string>().c_str());
        }

        auto ProductDatabaseEntry::GetColumns()
        {
            return SQLite::MakeColumns(
                SQLite::MakeColumn("ProductID", m_productID),
                SQLite::MakeColumn("JobPK", m_jobPK),
                SQLite::MakeColumn("ProductName", m_productName),
                SQLite::MakeColumn("SubID", m_subID),
                SQLite::MakeColumn("AssetType", m_assetType),
                SQLite::MakeColumn("LegacyGuid", m_legacyGuid)
            );
        }

        /////////////////////////////
        // LegacySubIDsEntry
        // loaded from db, and thus includes the PK
        LegacySubIDsEntry::LegacySubIDsEntry(AZ::s64 subIDsEntryID, AZ::s64 productPK, AZ::u32 subId)
            : m_subIDsEntryID(subIDsEntryID)
            , m_productPK(productPK)
            , m_subID(subId)
        {
        }

        LegacySubIDsEntry::LegacySubIDsEntry(AZ::s64 productPK, AZ::u32 subId)
            : m_productPK(productPK)
            , m_subID(subId)
        {
        }

        auto LegacySubIDsEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("LegacySubID", m_subIDsEntryID),
                MakeColumn("ProductPK", m_productPK),
                MakeColumn("SubID", m_subID)
            );
        }

        //////////////////////////////////////////////////////////////////////////
        //ProductDepdendencyDatabaseEntry
        ProductDependencyDatabaseEntry::ProductDependencyDatabaseEntry(AZ::s64 productDependencyID, AZ::s64 productPK, AZ::Uuid dependencySourceGuid, AZ::u32 dependencySubID, AZStd::bitset<64> dependencyFlags, const AZStd::string& platform, const AZStd::string& unresolvedPath, DependencyType dependencyType)
            : m_productDependencyID(productDependencyID)
            , m_productPK(productPK)
            , m_dependencySourceGuid(dependencySourceGuid)
            , m_dependencySubID(dependencySubID)
            , m_dependencyFlags(dependencyFlags)
            , m_platform(platform)
            , m_unresolvedPath(unresolvedPath)
            , m_dependencyType(dependencyType)
        {
        }

        ProductDependencyDatabaseEntry::ProductDependencyDatabaseEntry(AZ::s64 productPK, AZ::Uuid dependencySourceGuid, AZ::u32 dependencySubID, AZStd::bitset<64> dependencyFlags, const AZStd::string& platform, const AZStd::string& unresolvedPath, DependencyType dependencyType)
            : m_productDependencyID(-1)
            , m_productPK(productPK)
            , m_dependencySourceGuid(dependencySourceGuid)
            , m_dependencySubID(dependencySubID)
            , m_dependencyFlags(dependencyFlags)
            , m_platform(platform)
            , m_unresolvedPath(unresolvedPath)
            , m_dependencyType(dependencyType)
        {
        }

        bool ProductDependencyDatabaseEntry::operator==(const ProductDependencyDatabaseEntry& other) const
        {
            //equivalence is when everything but the id is the same
            return m_productPK == other.m_productPK &&
                   m_dependencySourceGuid == other.m_dependencySourceGuid &&
                   m_dependencySubID == other.m_dependencySubID &&
                   m_dependencyFlags == other.m_dependencyFlags &&
                   m_unresolvedPath == other.m_unresolvedPath &&
                   m_dependencyType == other.m_dependencyType &&
                   m_platform == other.m_platform;
        }

        AZStd::string ProductDependencyDatabaseEntry::ToString() const
        {
            return AZStd::string::format("ProductDependencyDatabaseEntry id: %i productpk: %i dependencysourceguid: %s dependencysubid: %i dependencyflags: %i unresolvedPath: %s dependencyType: %i", m_productDependencyID, m_productPK, m_dependencySourceGuid.ToString<AZStd::string>().c_str(), m_dependencySubID, m_dependencyFlags, m_unresolvedPath.c_str(), m_dependencyType);
        }

        auto ProductDependencyDatabaseEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("ProductDependencyID", m_productDependencyID),
                MakeColumn("ProductPK", m_productPK),
                MakeColumn("DependencySourceGuid", m_dependencySourceGuid),
                MakeColumn("DependencySubID", m_dependencySubID),
                MakeColumn("DependencyFlags", m_dependencyFlags),
                MakeColumn("Platform", m_platform),
                MakeColumn("UnresolvedPath", m_unresolvedPath),
                MakeColumn("UnresolvedDependencyType", m_dependencyType)
            );
        }

        //////////////////////////////////////////////////////////////////////////
        //FileDatabaseEntry

        bool FileDatabaseEntry::operator==(const FileDatabaseEntry& other) const
        {
            return m_scanFolderPK == other.m_scanFolderPK
                && m_fileName == other.m_fileName
                && m_isFolder == other.m_isFolder
                && m_modTime == other.m_modTime;
        }

        AZStd::string FileDatabaseEntry::ToString() const
        {
            return AZStd::string::format("FileDatabaseEntry id: %i scanfolderpk: %i filename: %s isfolder: %i modtime: %i",
                m_fileID, m_scanFolderPK, m_fileName.c_str(), m_isFolder, m_modTime);
        }

        auto FileDatabaseEntry::GetColumns()
        {
            return MakeColumns(
                MakeColumn("FileID", m_fileID),
                MakeColumn("ScanFolderPK", m_scanFolderPK),
                MakeColumn("FileName", m_fileName),
                MakeColumn("IsFolder", m_isFolder),
                MakeColumn("ModTime", m_modTime)
            );
        }

        //////////////////////////////////////////////////////////////////////////

        auto SourceAndScanFolderDatabaseEntry::GetColumns()
        {
            return CombineColumns(
                ScanFolderDatabaseEntry::GetColumns(),
                SourceDatabaseEntry::GetColumns()
            );
        }

        auto CombinedDatabaseEntry::GetColumns()
        {
            return CombineColumns(ScanFolderDatabaseEntry::GetColumns(),
                SourceDatabaseEntry::GetColumns(),
                JobDatabaseEntry::GetColumns(),
                ProductDatabaseEntry::GetColumns());
        }

        //////////////////////////////////////////////////////////////////////////
        //AssetDatabaseConnection
        AssetDatabaseConnection::AssetDatabaseConnection()
            : m_databaseConnection(nullptr)
        {
        }

        AssetDatabaseConnection::~AssetDatabaseConnection()
        {
            CloseDatabase();
        }

        void AssetDatabaseConnection::CloseDatabase()
        {
            if (m_databaseConnection)
            {
                m_databaseConnection->Close();
                delete m_databaseConnection;
                m_databaseConnection = nullptr;
            }
            m_validatedTables.clear();
        }

        AZStd::string AssetDatabaseConnection::GetAssetDatabaseFilePath()
        {
            AZStd::string databaseLocation;
            EBUS_EVENT(AssetDatabaseRequests::Bus, GetAssetDatabaseLocation, databaseLocation);
            if (databaseLocation.empty())
            {
                databaseLocation = "assetdb.sqlite";
            }
            return databaseLocation;
        }

        bool AssetDatabaseConnection::OpenDatabase()
        {
            AZ_Assert(!m_databaseConnection, "Already open!");
            AZStd::string assetDatabaseLocation = GetAssetDatabaseFilePath();
            AZStd::string parentFolder = assetDatabaseLocation;
            AzFramework::StringFunc::Path::StripFullName(parentFolder);
            if (!parentFolder.empty())
            {
                AZ::IO::SystemFile::CreateDir(parentFolder.c_str());
            }

            if ((IsReadOnly()) && (!AZ::IO::SystemFile::Exists(assetDatabaseLocation.c_str())))
            {
                AZ_Error("Connection", false, "There is no asset data base in the cache folder.  Cannot open the database.  Make sure Asset Processor is running.");
                return false;
            }

            if (!IsReadOnly() && AZ::IO::SystemFile::Exists(assetDatabaseLocation.c_str()) && !AZ::IO::SystemFile::IsWritable(assetDatabaseLocation.c_str()))
            {
                AZ_Error("Connection", false, "Asset database file %s is marked read-only.  The cache should not be checked into source control.", assetDatabaseLocation.c_str());
                return false;
            }

            m_databaseConnection = aznew SQLite::Connection();
            AZ_Assert(m_databaseConnection, "No database created");

            if (!m_databaseConnection->Open(assetDatabaseLocation, IsReadOnly()))
            {
                delete m_databaseConnection;
                m_databaseConnection = nullptr;
                AZ_Warning("Connection", false, "Unable to open the asset database at %s\n", assetDatabaseLocation.c_str());
                return false;
            }

            m_validatedTables.clear();
            CreateStatements();

            if (!PostOpenDatabase())
            {
                CloseDatabase();
                return false;
            }

            return true;
        }

        bool AssetDatabaseConnection::PostOpenDatabase()
        {
            if (QueryDatabaseVersion() != CurrentDatabaseVersion())
            {
                AZ_Error(LOG_NAME, false, "Unable to open database - invalid version - database has %i and we want %i\n", QueryDatabaseVersion(), CurrentDatabaseVersion());
                return false;
            }

            // make sure that we can see all required tables.
            for (size_t tableIndex = 0; tableIndex < AZ_ARRAY_SIZE(EXPECTED_TABLES); ++tableIndex)
            {
                if (!ValidateDatabaseTable("PostOpenDatabase", EXPECTED_TABLES[tableIndex]))
                {
                    AZ_Error(LOG_NAME, false, "The asset database in the Cache folder appears to be from a newer version of Asset Processor.  The Asset Processor will close, to prevent data loss.\n");
                    return false;
                }
            }

            return true;
        }

        void AssetDatabaseConnection::CreateStatements()
        {
            AZ_Assert(m_databaseConnection, "No connection!");
            AZ_Assert(m_databaseConnection->IsOpen(), "Connection is not open");

            //////////////////////////////////////////////////////////////////////////
            //table queries
            AddStatement(m_databaseConnection, s_queryDatabaseinfoTable);
            AddStatement(m_databaseConnection, s_queryScanfoldersTable);
            AddStatement(m_databaseConnection, s_querySourcesTable);
            AddStatement(m_databaseConnection, s_queryJobsTable);
            AddStatement(m_databaseConnection, s_queryJobsTablePlatform);
            AddStatement(m_databaseConnection, s_queryProductsTable);
            AddStatement(m_databaseConnection, s_queryProductsTablePlatform);
            AddStatement(m_databaseConnection, s_queryLegacysubidsbyproductid);
            AddStatement(m_databaseConnection, s_queryProductdependenciesTable);
            AddStatement(m_databaseConnection, s_queryFilesTable);

            //////////////////////////////////////////////////////////////////////////
            //projection and combination queries
            AddStatement(m_databaseConnection, s_queryScanfolderByScanfolderid);
            AddStatement(m_databaseConnection, s_queryScanfolderByDisplayname);
            AddStatement(m_databaseConnection, s_queryScanfolderByPortablekey);

            AddStatement(m_databaseConnection, s_querySourceBySourceid);
            AddStatement(m_databaseConnection, s_querySourceByScanfolderid);
            AddStatement(m_databaseConnection, s_querySourceBySourceguid);

            AddStatement(m_databaseConnection, s_querySourceBySourcename);
            AddStatement(m_databaseConnection, s_querySourceBySourcenameScanfolderid);
            AddStatement(m_databaseConnection, s_querySourceLikeSourcename);
            AddStatement(m_databaseConnection, s_querySourceAnalysisFingerprint);
            AddStatement(m_databaseConnection, s_querySourcesAndScanfolders);

            AddStatement(m_databaseConnection, s_queryJobByJobid);
            AddStatement(m_databaseConnection, s_queryJobByJobkey);
            AddStatement(m_databaseConnection, s_queryJobByJobrunkey);
            AddStatement(m_databaseConnection, s_queryJobByProductid);
            AddStatement(m_databaseConnection, s_queryJobBySourceid);
            AddStatement(m_databaseConnection, s_queryJobBySourceidPlatform);

            AddStatement(m_databaseConnection, s_queryProductByProductid);
            AddStatement(m_databaseConnection, s_queryProductByJobid);
            AddStatement(m_databaseConnection, s_queryProductByJobidPlatform);
            AddStatement(m_databaseConnection, s_queryProductBySourceid);
            AddStatement(m_databaseConnection, s_queryProductBySourceidPlatform);

            AddStatement(m_databaseConnection, s_queryProductByProductname);
            AddStatement(m_databaseConnection, s_queryProductByProductnamePlatform);
            AddStatement(m_databaseConnection, s_queryProductLikeProductname);
            AddStatement(m_databaseConnection, s_queryProductLikeProductnamePlatform);

            AddStatement(m_databaseConnection, s_queryProductBySourcename);
            AddStatement(m_databaseConnection, s_queryProductBySourcenamePlatform);
            AddStatement(m_databaseConnection, s_queryProductLikeSourcename);
            AddStatement(m_databaseConnection, s_queryProductLikeSourcenamePlatform);
            AddStatement(m_databaseConnection, s_queryProductByJobIdSubId);
            AddStatement(m_databaseConnection, s_queryProductBySourceGuidSubid);

            AddStatement(m_databaseConnection, s_queryCombined);
            AddStatement(m_databaseConnection, s_queryCombinedByPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedBySourceid);
            AddStatement(m_databaseConnection, s_queryCombinedBySourceidPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedByJobid);
            AddStatement(m_databaseConnection, s_queryCombinedByJobidPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedByProductid);
            AddStatement(m_databaseConnection, s_queryCombinedByProductidPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedBySourceguidProductsubid);
            AddStatement(m_databaseConnection, s_queryCombinedBySourceguidProductsubidPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedBySourcename);
            AddStatement(m_databaseConnection, s_queryCombinedBySourcenamePlatform);

            AddStatement(m_databaseConnection, s_queryCombinedLikeSourcename);
            AddStatement(m_databaseConnection, s_queryCombinedLikeSourcenamePlatform);

            AddStatement(m_databaseConnection, s_queryCombinedByProductname);
            AddStatement(m_databaseConnection, s_queryCombinedByProductnamePlatform);

            AddStatement(m_databaseConnection, s_queryCombinedLikeProductname);
            AddStatement(m_databaseConnection, s_queryCombinedLikeProductnamePlatform);

            AddStatement(m_databaseConnection, s_querySourcedependencyBySourcedependencyid);
            AddStatement(m_databaseConnection, s_querySourcedependencyByDependsonsource);
            AddStatement(m_databaseConnection, s_querySourcedependencyByDependsonsourceWildcard);
            AddStatement(m_databaseConnection, s_queryDependsonsourceBySource);

            AddStatement(m_databaseConnection, s_queryProductdependencyByProductdependencyid);
            AddStatement(m_databaseConnection, s_queryProductdependencyByProductid);
            AddStatement(m_databaseConnection, s_queryDirectProductdependencies);
            AddStatement(m_databaseConnection, s_queryAllProductdependencies);
            AddStatement(m_databaseConnection, s_queryUnresolvedProductDependencies);

            AddStatement(m_databaseConnection, s_queryFileByFileid);
            AddStatement(m_databaseConnection, s_queryFilesByFileName);
            AddStatement(m_databaseConnection, s_queryFilesLikeFileName);
            AddStatement(m_databaseConnection, s_queryFilesByScanfolderid);
            AddStatement(m_databaseConnection, s_queryFileByFileNameScanfolderid);

            AddStatement(m_databaseConnection, s_queryBuilderInfoTable);
        }

        //////////////////////////////////////////////////////////////////////////
        //Like
        AZStd::string AssetDatabaseConnection::GetLikeActualSearchTerm(const char* likeString, LikeType likeType)
        {
            AZStd::string actualSearchTerm = likeString;
            if (likeType == StartsWith)
            {
                StringFunc::Replace(actualSearchTerm, "%", "|%");
                StringFunc::Replace(actualSearchTerm, "_", "|_");
                StringFunc::Append(actualSearchTerm, "%");
            }
            else if (likeType == EndsWith)
            {
                StringFunc::Replace(actualSearchTerm, "%", "|%");
                StringFunc::Replace(actualSearchTerm, "_", "|_");
                StringFunc::Prepend(actualSearchTerm, "%");
            }
            else if (likeType == Matches)
            {
                StringFunc::Replace(actualSearchTerm, "%", "|%");
                StringFunc::Replace(actualSearchTerm, "_", "|_");
                StringFunc::Prepend(actualSearchTerm, "%");
                StringFunc::Append(actualSearchTerm, "%");
            }
            //raw default

            return actualSearchTerm;
        }

        //////////////////////////////////////////////////////////////////////////
        //Table queries

        bool AssetDatabaseConnection::QueryDatabaseInfoTable(databaseInfoHandler handler)
        {
            return s_queryDatabaseinfoTable.BindAndQuery(*m_databaseConnection, handler, &GetDatabaseInfoResult);
        }

        DatabaseVersion AssetDatabaseConnection::QueryDatabaseVersion()
        {
            DatabaseVersion dbVersion;
            bool res = QueryDatabaseInfoTable(
                    [&](DatabaseInfoEntry& entry)
                    {
                        dbVersion = entry.m_version;
                        return true; //see all of them
                    });

            if (res)
            {
                return dbVersion;
            }

            return DatabaseVersion::DatabaseDoesNotExist;
        }

        bool AssetDatabaseConnection::QueryScanFoldersTable(scanFolderHandler handler)
        {
            return s_queryScanfoldersTable.BindAndQuery(*m_databaseConnection, handler, &GetScanFolderResult);
        }

        bool AssetDatabaseConnection::QuerySourcesTable(sourceHandler handler)
        {
            return s_querySourcesTable.BindAndQuery(*m_databaseConnection, handler, &GetSourceResult);
        }

        bool AssetDatabaseConnection::QueryJobsTable(jobHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryJobsTablePlatform.BindAndThen(*m_databaseConnection, handler, platform).Query(&GetJobResult, builderGuid, jobKey, status);
            }
            
            return s_queryJobsTable.BindAndThen(*m_databaseConnection, handler).Query(&GetJobResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductsTable(productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryProductsTablePlatform.BindAndThen(*m_databaseConnection, handler, platform).Query(&GetProductResult, builderGuid, jobKey, status);
            }
            
            return s_queryProductsTable.BindAndThen(*m_databaseConnection, handler).Query(&GetProductResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductDependenciesTable(combinedProductDependencyHandler handler)
        {
            return s_queryProductdependenciesTable.BindAndQuery(*m_databaseConnection, handler, &GetCombinedDependencyResult);
        }

        bool AssetDatabaseConnection::QueryFilesTable(fileHandler handler)
        {
            return s_queryFilesTable.BindAndQuery(*m_databaseConnection, handler, &GetFileResult);
        }

        bool AssetDatabaseConnection::QueryScanFolderByScanFolderID(AZ::s64 scanfolderid, scanFolderHandler handler)
        {
            return s_queryScanfolderByScanfolderid.BindAndQuery(*m_databaseConnection, handler, &GetScanFolderResult, scanfolderid);
        }

        bool AssetDatabaseConnection::QueryScanFolderBySourceID(AZ::s64 sourceID, scanFolderHandler handler)
        {
            bool found = false;
            bool succeeded = QueryCombinedBySourceID(sourceID,
                    [&](CombinedDatabaseEntry& combined)
                    {
                        found = true;
                        ScanFolderDatabaseEntry scanFolder = AZStd::move(combined);
                        return handler(scanFolder);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryScanFolderByJobID(AZ::s64 jobID, scanFolderHandler handler)
        {
            bool found = false;
            bool succeeded = QueryCombinedByJobID(jobID,
                    [&](CombinedDatabaseEntry& combined)
                    {
                        found = true;
                        ScanFolderDatabaseEntry scanFolder = AZStd::move(combined);
                        return handler(scanFolder);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryScanFolderByProductID(AZ::s64 productID, scanFolderHandler handler)
        {
            bool found = false;
            bool succeeded = QueryCombinedBySourceID(productID,
                    [&](CombinedDatabaseEntry& combined)
                    {
                        found = true;
                        ScanFolderDatabaseEntry scanFolder = AZStd::move(combined);
                        return handler(scanFolder);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryScanFolderByDisplayName(const char* displayName, scanFolderHandler handler)
        {
            return s_queryScanfolderByDisplayname.BindAndQuery(*m_databaseConnection, handler, &GetScanFolderResult, displayName);
        }

        bool AssetDatabaseConnection::QueryScanFolderByPortableKey(const char* portableKey, scanFolderHandler handler)
        {
            return s_queryScanfolderByPortablekey.BindAndQuery(*m_databaseConnection, handler, &GetScanFolderResult, portableKey);
        }

        bool AssetDatabaseConnection::QuerySourceBySourceID(AZ::s64 sourceid, sourceHandler handler)
        {
            return s_querySourceBySourceid.BindAndQuery(*m_databaseConnection, handler, &GetSourceResult, sourceid);
        }

        bool AssetDatabaseConnection::QuerySourceByScanFolderID(AZ::s64 scanFolderID, sourceHandler handler)
        {
            return s_querySourceByScanfolderid.BindAndQuery(*m_databaseConnection, handler, &GetSourceResult, scanFolderID);
        }

        bool AssetDatabaseConnection::QuerySourceByJobID(AZ::s64 jobid, sourceHandler handler)
        {
            return QueryCombinedByJobID(jobid,
                [&](CombinedDatabaseEntry& combined)
                {
                    SourceDatabaseEntry source;
                    source = AZStd::move(combined);
                    handler(source);
                    return false;//one
                }, nullptr,
                nullptr);
        }

        bool AssetDatabaseConnection::QuerySourceByProductID(AZ::s64 productid, sourceHandler handler)
        {
            return QueryCombinedByProductID(productid,
                [&](CombinedDatabaseEntry& combined)
                {
                    SourceDatabaseEntry source;
                    source = AZStd::move(combined);
                    handler(source);
                    return false;//one
                }, nullptr);
        }

        bool AssetDatabaseConnection::QuerySourceBySourceGuid(AZ::Uuid sourceGuid, sourceHandler handler)
        {
            return s_querySourceBySourceguid.BindAndQuery(*m_databaseConnection, handler, &GetSourceResult, sourceGuid);
        }

        bool AssetDatabaseConnection::QuerySourceBySourceName(const char* exactSourceName, sourceHandler handler)
        {
            return s_querySourceBySourcename.BindAndQuery(*m_databaseConnection, handler, &GetSourceResult, exactSourceName);
        }

        bool AssetDatabaseConnection::QuerySourceAnalysisFingerprint(const char* exactSourceName, AZ::s64 scanFolderID, AZStd::string& outFingerprint)
        {
            outFingerprint.clear();

            StatementAutoFinalizer autoFinal;
            if (!s_querySourceAnalysisFingerprint.Bind(*m_databaseConnection, autoFinal, exactSourceName, scanFolderID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();
            Statement::SqlStatus result = statement->Step();

            bool validResult = result == Statement::SqlDone; // this means no results, but no error
            if (result == Statement::SqlOK) // this means results, no error
            {
                // this is a highly optimized query and always results in only one column.
                outFingerprint = statement->GetColumnText(0);
                validResult = true;
            }
            return validResult;
        }

        bool AssetDatabaseConnection::QuerySourceAndScanfolder(combinedSourceScanFolderHandler handler)
        {
            return s_querySourcesAndScanfolders.BindAndQuery(*m_databaseConnection, handler, &GetSourceAndScanfolderResult);
        }

        bool AssetDatabaseConnection::QuerySourceBySourceNameScanFolderID(const char* exactSourceName, AZ::s64 scanFolderID, sourceHandler handler)
        {
            return s_querySourceBySourcenameScanfolderid.BindAndQuery(*m_databaseConnection, handler, &GetSourceResult, exactSourceName, scanFolderID);
        }

        bool AssetDatabaseConnection::QuerySourceLikeSourceName(const char* likeSourceName, LikeType likeType, sourceHandler handler)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeSourceName, likeType);

            return s_querySourceLikeSourcename.BindAndQuery(*m_databaseConnection, handler, &GetSourceResult, actualSearchTerm.c_str());
        }

        bool AssetDatabaseConnection::QueryJobByJobID(AZ::s64 jobid, jobHandler handler)
        {
            return s_queryJobByJobid.BindAndQuery(*m_databaseConnection, handler, &GetJobResultSimple, jobid);
        }

        bool AssetDatabaseConnection::QueryJobByJobKey(AZStd::string jobKey, jobHandler handler)
        {
            return s_queryJobByJobkey.BindAndQuery(*m_databaseConnection, handler, &GetJobResultSimple, jobKey.c_str());
        }

        bool AssetDatabaseConnection::QueryJobByJobRunKey(AZ::u64 jobrunkey, jobHandler handler)
        {
            return s_queryJobByJobrunkey.BindAndQuery(*m_databaseConnection, handler, &GetJobResultSimple, jobrunkey);
        }

        bool AssetDatabaseConnection::QueryJobByProductID(AZ::s64 productid, jobHandler handler)
        {
            return s_queryJobByProductid.BindAndQuery(*m_databaseConnection, handler, &GetJobResultSimple, productid);
        }

        bool AssetDatabaseConnection::QueryJobBySourceID(AZ::s64 sourceID, jobHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryJobBySourceidPlatform.BindAndThen(*m_databaseConnection, handler, sourceID, platform).Query(&GetJobResult, builderGuid, jobKey, status);
            }
            
            return s_queryJobBySourceid.BindAndThen(*m_databaseConnection, handler, sourceID).Query(&GetJobResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductByProductID(AZ::s64 productid, productHandler handler)
        {
            return s_queryProductByProductid.BindAndQuery(*m_databaseConnection, handler, &GetProductResultSimple, productid);
        }

        bool AssetDatabaseConnection::QueryProductByJobID(AZ::s64 jobid, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryProductByJobidPlatform.BindAndThen(*m_databaseConnection, handler, jobid, platform).Query(&GetProductResult, builderGuid, jobKey, status);
            }
            
            return s_queryProductByJobid.BindAndThen(*m_databaseConnection, handler, jobid).Query(&GetProductResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductBySourceID(AZ::s64 sourceid, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryProductBySourceidPlatform.BindAndThen(*m_databaseConnection, handler, sourceid, platform).Query(&GetProductResult, builderGuid, jobKey, status);
            }
            
            return s_queryProductBySourceid.BindAndThen(*m_databaseConnection, handler, sourceid).Query(&GetProductResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductBySourceGuidSubID(AZ::Uuid sourceGuid, AZ::u32 productSubID, productHandler handler)
        {
            return s_queryProductBySourceGuidSubid.BindAndQuery(*m_databaseConnection, handler, &GetProductResultSimple, sourceGuid, productSubID);
        }

        bool AssetDatabaseConnection::QueryProductByProductName(const char* exactProductname, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryProductByProductnamePlatform.BindAndThen(*m_databaseConnection, handler, exactProductname, platform).Query(&GetProductResult, builderGuid, jobKey, status);
            }
            
            return s_queryProductByProductname.BindAndThen(*m_databaseConnection, handler, exactProductname).Query(&GetProductResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductLikeProductName(const char* likeProductname, LikeType likeType, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeProductname, likeType);

            if (platform && strlen(platform))
            {
                return s_queryProductLikeProductnamePlatform.BindAndThen(*m_databaseConnection, handler, actualSearchTerm.c_str(), platform).Query(&GetProductResult, builderGuid, jobKey, status);
            }
            
            return s_queryProductLikeProductname.BindAndThen(*m_databaseConnection, handler, actualSearchTerm.c_str()).Query(&GetProductResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductBySourceName(const char* exactSourceName, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryProductBySourcenamePlatform.BindAndThen(*m_databaseConnection, handler, exactSourceName, platform).Query(&GetProductResult, builderGuid, jobKey, status);
            }
            
            return s_queryProductBySourcename.BindAndThen(*m_databaseConnection, handler, exactSourceName).Query(&GetProductResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductLikeSourceName(const char* likeSourceName, LikeType likeType, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeSourceName, likeType);

            if (platform && strlen(platform))
            {
                return s_queryProductLikeSourcenamePlatform.BindAndThen(*m_databaseConnection, handler, actualSearchTerm.c_str(), platform).Query(&GetProductResult, builderGuid, jobKey, status);
            }
            
            return s_queryProductLikeSourcename.BindAndThen(*m_databaseConnection, handler, actualSearchTerm.c_str()).Query(&GetProductResult, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductByJobIDSubID(AZ::s64 jobID, AZ::u32 subId, productHandler handler)
        {
            return s_queryProductByJobIdSubId.BindAndQuery(*m_databaseConnection, handler, &GetProductResultSimple, jobID, subId);
        }

        bool AssetDatabaseConnection::QueryLegacySubIdsByProductID(AZ::s64 productId, legacySubIDsHandler handler)
        {
            return s_queryLegacysubidsbyproductid.BindAndQuery(*m_databaseConnection, handler, &GetLegacySubIDsResult, productId);
        }

        bool AssetDatabaseConnection::QueryCombined(combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status, bool includeLegacySubIDs)
        {
            using namespace AZStd::placeholders;
            auto callback = AZStd::bind(&AssetDatabaseConnection::GetCombinedResult, this, _1, _2, _3, builderGuid, jobKey, status, includeLegacySubIDs);

            if (platform && strlen(platform))
            {
                return s_queryCombinedByPlatform.BindAndQuery(*m_databaseConnection, handler, callback, platform);
            }
            
            return s_queryCombined.BindAndQuery(*m_databaseConnection, handler, callback);
        }

        auto AssetDatabaseConnection::GetCombinedResultAsLambda()
        {
            return [this](const char* name, Statement* statement, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, AzToolsFramework::AssetSystem::JobStatus status)
            {
                return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
            };
        }

        bool AssetDatabaseConnection::QueryCombinedBySourceID(AZ::s64 sourceID, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryCombinedBySourceidPlatform.BindAndThen(*m_databaseConnection, handler, sourceID, platform)
                    .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
            }
            
            return s_queryCombinedBySourceid.BindAndThen(*m_databaseConnection, handler, sourceID)
                .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedByJobID(AZ::s64 jobID, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryCombinedByJobidPlatform.BindAndThen(*m_databaseConnection, handler, jobID, platform)
                    .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
            }
            
            return s_queryCombinedByJobid.BindAndThen(*m_databaseConnection, handler, jobID)
                .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedByProductID(AZ::s64 productID, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryCombinedByProductidPlatform.BindAndThen(*m_databaseConnection, handler, productID, platform)
                    .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
            }
            
            return s_queryCombinedByProductid.BindAndThen(*m_databaseConnection, handler, productID)
                .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedBySourceGuidProductSubId(AZ::Uuid sourceGuid, AZ::u32 productSubID, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryCombinedBySourceguidProductsubidPlatform.BindAndThen(*m_databaseConnection, handler, productSubID, sourceGuid, platform)
                    .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
            }
            
            return s_queryCombinedBySourceguidProductsubid.BindAndThen(*m_databaseConnection, handler, productSubID, sourceGuid)
                .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedBySourceName(const char* exactSourceName, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryCombinedBySourcenamePlatform.BindAndThen(*m_databaseConnection, handler, exactSourceName, platform)
                    .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
            }
            
            return s_queryCombinedBySourcename.BindAndThen(*m_databaseConnection, handler, exactSourceName)
                .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedLikeSourceName(const char* likeSourceName, LikeType likeType, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeSourceName, likeType);

            if (platform && strlen(platform))
            {
                return s_queryCombinedLikeSourcenamePlatform.BindAndThen(*m_databaseConnection, handler, actualSearchTerm.c_str(), platform)
                    .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
            }
            
            return s_queryCombinedLikeSourcename.BindAndThen(*m_databaseConnection, handler, actualSearchTerm.c_str())
                .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedByProductName(const char* exactProductName, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            if (platform && strlen(platform))
            {
                return s_queryCombinedByProductnamePlatform.BindAndThen(*m_databaseConnection, handler, exactProductName, platform)
                    .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
            }
            
            return s_queryCombinedByProductname.BindAndThen(*m_databaseConnection, handler, exactProductName)
                .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedLikeProductName(const char* likeProductName, LikeType likeType, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeProductName, likeType);

            if (platform && strlen(platform))
            {
                return s_queryCombinedLikeProductnamePlatform.BindAndThen(*m_databaseConnection, handler, actualSearchTerm.c_str(), platform)
                    .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
            }
            
            return s_queryCombinedLikeProductname.BindAndThen(*m_databaseConnection, handler, actualSearchTerm.c_str())
                .Query(GetCombinedResultAsLambda(), builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryJobInfoByJobID(AZ::s64 jobID, jobInfoHandler handler)
        {
            SourceDatabaseEntry source;

            bool found = false;
            bool succeeded = QuerySourceByJobID(jobID,
                    [&](SourceDatabaseEntry& entry)
                    {
                        found = true;
                        source = AZStd::move(entry);
                        return false;//one
                    });

            if (!found || !succeeded)
            {
                return false;
            }

            found = false;
            succeeded = QueryJobByJobID(jobID,
                    [&](JobDatabaseEntry& entry)
                    {
                        found = true;
                        AzToolsFramework::AssetSystem::JobInfo jobinfo;
                        jobinfo.m_sourceFile = source.m_sourceName;
                        PopulateJobInfo(jobinfo, entry);
                        return handler(jobinfo);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryJobInfoByJobRunKey(AZ::u64 jobRunKey, jobInfoHandler handler)
        {
            bool found = false;
            bool succeeded = QueryJobByJobRunKey(jobRunKey,
                    [&](JobDatabaseEntry& entry)
                    {
                        AzToolsFramework::AssetSystem::JobInfo jobinfo;
                        succeeded = QuerySourceBySourceID(entry.m_sourcePK,
                                [&](SourceDatabaseEntry& sourceEntry)
                                {
                                    found = true;
                                    jobinfo.m_sourceFile = AZStd::move(sourceEntry.m_sourceName);
                                    return true;
                                });

                        if (!found)
                        {
                            return false;
                        }

                        PopulateJobInfo(jobinfo, entry);

                        return handler(jobinfo);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryJobInfoByJobKey(AZStd::string jobKey, jobInfoHandler handler)
        {
            bool found = false;
            bool succeeded = QueryJobByJobKey(jobKey,
                    [&](JobDatabaseEntry& entry)
                    {
                        AzToolsFramework::AssetSystem::JobInfo jobinfo;
                        succeeded = QuerySourceBySourceID(entry.m_sourcePK,
                                [&](SourceDatabaseEntry& sourceEntry)
                                {
                                    jobinfo.m_sourceFile = AZStd::move(sourceEntry.m_sourceName);
                                    QueryScanFolderBySourceID(sourceEntry.m_sourceID,
                                        [&](ScanFolderDatabaseEntry& scanFolderEntry)
                                        {
                                            found = true;
                                            jobinfo.m_watchFolder = scanFolderEntry.m_scanFolder;
                                            return false;
                                        });
                                    return true;
                                });

                        if (!found)
                        {
                            return false;
                        }

                        PopulateJobInfo(jobinfo, entry);

                        return handler(jobinfo);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryJobInfoBySourceName(const char* sourceName, jobInfoHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            SourceDatabaseEntry source;

            bool found = false;
            bool succeeded = QuerySourceBySourceName(sourceName,
                    [&](SourceDatabaseEntry& entry)
                    {
                        found = true;
                        source = AZStd::move(entry);
                        return false;//one
                    });

            if (!found || !succeeded)
            {
                return false;
            }

            found = false;
            succeeded = QueryJobBySourceID(source.m_sourceID,
                    [&](JobDatabaseEntry& entry)
                    {
                        AzToolsFramework::AssetSystem::JobInfo jobinfo;
                        jobinfo.m_sourceFile = source.m_sourceName; //dont move, we may have many that need this name
                        QueryScanFolderBySourceID(source.m_sourceID,
                            [&](ScanFolderDatabaseEntry& scanFolderEntry)
                            {
                                found = true;
                                jobinfo.m_watchFolder = scanFolderEntry.m_scanFolder;
                                return false;
                            });
                        PopulateJobInfo(jobinfo, entry);

                        return handler(jobinfo);
                    }, builderGuid,
                    jobKey,
                    platform,
                    status);
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QuerySourceDependencyBySourceDependencyId(AZ::s64 sourceDependencyID, sourceFileDependencyHandler handler)
        {
            return s_querySourcedependencyBySourcedependencyid.BindAndQuery(*m_databaseConnection, handler, &GetSourceDependencyResult, sourceDependencyID);
        }

        bool AssetDatabaseConnection::QuerySourceDependencyByDependsOnSource(const char* dependsOnSource, const char* dependentFilter, AzToolsFramework::AssetDatabase::SourceFileDependencyEntry::TypeOfDependency dependencyType, sourceFileDependencyHandler handler)
        {
            if (dependencyType & AzToolsFramework::AssetDatabase::SourceFileDependencyEntry::DEP_SourceLikeMatch)
            {
                return QuerySourceDependencyByDependsOnSourceWildcard(dependsOnSource, dependentFilter, handler);
            }
            return s_querySourcedependencyByDependsonsource.BindAndQuery(*m_databaseConnection, handler, &GetSourceDependencyResult, dependsOnSource, dependentFilter == nullptr ? "%" : dependentFilter, dependencyType);
        }

        bool AssetDatabaseConnection::QuerySourceDependencyByDependsOnSourceWildcard(const char* dependsOnSource, const char* dependentFilter, sourceFileDependencyHandler handler)
        {
            SourceFileDependencyEntry::TypeOfDependency matchDependency = SourceFileDependencyEntry::TypeOfDependency::DEP_SourceOrJob;
            SourceFileDependencyEntry::TypeOfDependency wildcardDependency = SourceFileDependencyEntry::TypeOfDependency::DEP_SourceLikeMatch;

            return s_querySourcedependencyByDependsonsourceWildcard.BindAndQuery(*m_databaseConnection, handler, &GetSourceDependencyResult, dependsOnSource, dependentFilter == nullptr ? "%" : dependentFilter, matchDependency, wildcardDependency);
        }

        bool AssetDatabaseConnection::QueryDependsOnSourceBySourceDependency(const char* sourceDependency, const char* dependencyFilter, AzToolsFramework::AssetDatabase::SourceFileDependencyEntry::TypeOfDependency dependencyType, sourceFileDependencyHandler handler)
        {
            return s_queryDependsonsourceBySource.BindAndQuery(*m_databaseConnection, handler, &GetSourceDependencyResult, sourceDependency, dependencyFilter == nullptr ? "%" : dependencyFilter, dependencyType);
        }

        // Product Dependencies
        bool AssetDatabaseConnection::QueryProductDependencyByProductDependencyId(AZ::s64 productDependencyID, productDependencyHandler handler)
        {
            return s_queryProductdependencyByProductdependencyid.BindAndQuery(*m_databaseConnection, handler, &GetProductDependencyResult, productDependencyID);
        }

        bool AssetDatabaseConnection::QueryUnresolvedProductDependencies(productDependencyHandler handler)
        {
            return s_queryUnresolvedProductDependencies.BindAndQuery(*m_databaseConnection, handler, &GetProductDependencyResult);
        }

        bool AssetDatabaseConnection::QueryProductDependencyByProductId(AZ::s64 productID, productDependencyHandler handler)
        {
            return s_queryProductdependencyByProductid.BindAndQuery(*m_databaseConnection, handler, &GetProductDependencyResult, productID);
        }

        bool AssetDatabaseConnection::QueryDirectProductDependencies(AZ::s64 productID, productHandler handler)
        {
            return s_queryDirectProductdependencies.BindAndQuery(*m_databaseConnection, handler, &GetProductResultSimple, productID);
        }

        bool AssetDatabaseConnection::QueryAllProductDependencies(AZ::s64 productID, productHandler handler)
        {
            return s_queryAllProductdependencies.BindAndQuery(*m_databaseConnection, handler, &GetProductResultSimple, productID);
        }

        bool AssetDatabaseConnection::QueryFileByFileID(AZ::s64 fileID, fileHandler handler)
        {
            return s_queryFileByFileid.BindAndQuery(*m_databaseConnection, handler, &GetFileResult, fileID);
        }

        bool AssetDatabaseConnection::QueryFilesByFileNameAndScanFolderID(const char* fileName, AZ::s64 scanFolderID, fileHandler handler)
        {
            return s_queryFilesByFileName.BindAndQuery(*m_databaseConnection, handler, &GetFileResult, scanFolderID, fileName);
        }

        bool AssetDatabaseConnection::QueryFilesLikeFileName(const char* likeFileName, LikeType likeType, fileHandler handler)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeFileName, likeType);

            return s_queryFilesLikeFileName.BindAndQuery(*m_databaseConnection, handler, &GetFileResult, actualSearchTerm.c_str());
        }

        bool AssetDatabaseConnection::QueryFilesByScanFolderID(AZ::s64 scanFolderID, fileHandler handler) 
        {
            return s_queryFilesByScanfolderid.BindAndQuery(*m_databaseConnection, handler, &GetFileResult, scanFolderID);
        }

        bool AssetDatabaseConnection::QueryFileByFileNameScanFolderID(const char* fileName, AZ::s64 scanFolderID, fileHandler handler)
        {
            return s_queryFileByFileNameScanfolderid.BindAndQuery(*m_databaseConnection, handler, &GetFileResult, scanFolderID, fileName);
        }

        bool AssetDatabaseConnection::QueryBuilderInfoTable(const BuilderInfoHandler& handler)
        {
            StatementAutoFinalizer autoFinal;
            if (!s_queryBuilderInfoTable.Bind(*m_databaseConnection, autoFinal))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            Statement::SqlStatus result = statement->Step();

            if (result == Statement::SqlError)
            {
                AZ_Error(LOG_NAME, false, "SqlError occurred!");
                return false;
            }

            BuilderInfoEntry entry;
            auto boundColumns = entry.GetColumns();

            // possible values for result:
            // SqlDone:  There is no more data available (but there was no error)
            // SqlOk: There is data available.
            while (result == Statement::SqlOK)
            {
                if(!boundColumns.Fetch(statement))
                {
                    return false;
                }

                if(!handler(BuilderInfoEntry(entry)))
                {
                    break; // handler returned false meaning "do not continue to return rows"
                }
                result = statement->Step();
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "SqlError occurred!");
                return false;
            }

            return true;
        }

        bool AssetDatabaseConnection::ValidateDatabaseTable(const char* callName, const char* tableName)
        {
            AZ_UNUSED(callName); // for release mode, when AZ_Error is compiled down to nothing.
            AZ_UNUSED(tableName);

            if (m_validatedTables.find(tableName) != m_validatedTables.end())
            {
                return true; // already validated.
            }

            if (!m_databaseConnection)
            {
                AZ_Error(LOG_NAME, false, "Fatal: attempt to work on a database connection that doesn't exist: %s", callName);
                return false;
            }

            if (!m_databaseConnection->IsOpen())
            {
                AZ_Error(LOG_NAME, false, "Fatal: attempt to work on a database connection that isn't open: %s", callName);
                return false;
            }

            if (!m_databaseConnection->DoesTableExist(tableName))
            {
                return false;
            }

            m_validatedTables.insert(tableName);

            return true;
        }

        // Simple helper function to determine if we should call the handler based on optional filtering criteria
        bool ResultMatchesJobCriteria(const char* jobKey, AZ::Uuid builderGuid, AssetSystem::JobStatus status, AZStd::string_view savedJobKey, AZ::Uuid savedBuilderGuid, AssetSystem::JobStatus savedJobStatus)
        {
            return (jobKey ? savedJobKey == jobKey : true)
                && (!builderGuid.IsNull() ? savedBuilderGuid == builderGuid : true)
                && (status != AssetSystem::JobStatus::Any ? savedJobStatus == status : true);
        }

        bool AssetDatabaseConnection::GetCombinedResult(const char* callName, Statement* statement, AssetDatabaseConnection::combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, AssetSystem::JobStatus status, bool includeLegacySubIDs)
        {
            AZ_UNUSED(callName); // AZ_Error may be compiled out entirely in release builds.
            Statement::SqlStatus result = statement->Step();

            CombinedDatabaseEntry combined;
            auto boundColumns = combined.GetColumns();

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                if (!boundColumns.Fetch(statement))
                {
                    return false;
                }

                if (ResultMatchesJobCriteria(jobKey, builderGuid, status, combined.m_jobKey, combined.m_builderGuid, combined.m_status))
                {
                    if (includeLegacySubIDs)
                    {
                        QueryLegacySubIdsByProductID(combined.m_productID, [&combined](LegacySubIDsEntry& entry)
                        {
                            combined.m_legacySubIDs.emplace_back(AZStd::move(entry));
                            return true;
                        }
                        );
                    }

                    if (handler(combined))
                    {
                        result = statement->Step();
                    }
                    else
                    {
                        result = Statement::SqlDone;
                    }
                }
                else
                {
                    result = statement->Step();
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }
            return validResult;
        }

        namespace
        {
            // Helper function that can handle the majority use-case of iterating every row and passing the TEntry result to the handler
            template<typename TEntry>
            bool GetResult(const char* callName, Statement* statement, AZStd::function<bool(TEntry& entry)> handler)
            {
                AZ_UNUSED(callName); // AZ_Error may be compiled out entirely in release builds.

                Statement::SqlStatus result = statement->Step();

                TEntry entry;
                auto boundColumns = entry.GetColumns();

                bool validResult = result == Statement::SqlDone;
                while (result == Statement::SqlOK)
                {
                    if (!boundColumns.Fetch(statement))
                    {
                        return false;
                    }

                    if (handler(entry))
                    {
                        result = statement->Step();
                    }
                    else
                    {
                        result = Statement::SqlDone;
                    }
                    validResult = true;
                }

                if (result == Statement::SqlError)
                {
                    AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                    return false;
                }

                return validResult;
            }

            bool GetDatabaseInfoResult(const char* callName, Statement* statement, AssetDatabaseConnection::databaseInfoHandler handler)
            {
                return GetResult(callName, statement, handler);
            }

            bool GetScanFolderResult(const char* callName, Statement* statement, AssetDatabaseConnection::scanFolderHandler handler)
            {
                return GetResult(callName, statement, handler);
            }

            bool GetSourceResult(const char* callName, Statement* statement, AssetDatabaseConnection::sourceHandler handler)
            {
                return GetResult(callName, statement, handler);
            }

            bool GetSourceAndScanfolderResult(const char* callName, Statement* statement, AssetDatabaseConnection::combinedSourceScanFolderHandler handler)
            {
                return GetResult(callName, statement, handler);
            }

            bool GetSourceDependencyResult(const char* callName, SQLite::Statement* statement, AssetDatabaseConnection::sourceFileDependencyHandler handler)
            {
                return GetResult(callName, statement, handler);
            }

            bool GetProductDependencyResult(const char* callName, Statement* statement, AssetDatabaseConnection::productDependencyHandler handler)
            {
                return GetResult(callName, statement, handler);
            }

            bool GetLegacySubIDsResult(const char* callName, SQLite::Statement* statement, AssetDatabaseConnection::legacySubIDsHandler handler)
            {
                return GetResult(callName, statement, handler);
            }

            bool GetFileResult(const char* callName, SQLite::Statement* statement, AssetDatabaseConnection::fileHandler handler)
            {
                return GetResult(callName, statement, handler);
            }

            bool GetJobResultSimple(const char* name, Statement* statement, AssetDatabaseConnection::jobHandler handler)
            {
                return GetJobResult(name, statement, handler);
            }

            bool GetJobResult(const char* callName, Statement* statement, AssetDatabaseConnection::jobHandler handler, AZ::Uuid builderGuid, const char* jobKey, AssetSystem::JobStatus status)
            {
                AZ_UNUSED(callName); // AZ_Error may be compiled out entirely in release builds.

                Statement::SqlStatus result = statement->Step();

                JobDatabaseEntry job;
                auto boundColumns = job.GetColumns();

                bool validResult = result == Statement::SqlDone;
                while (result == Statement::SqlOK)
                {
                    if (!boundColumns.Fetch(statement))
                    {
                        return false;
                    }

                    if (ResultMatchesJobCriteria(jobKey, builderGuid, status, job.m_jobKey, job.m_builderGuid, job.m_status))
                    {
                        if (handler(job))
                        {
                            result = statement->Step();
                        }
                        else
                        {
                            result = Statement::SqlDone;
                        }
                    }
                    else
                    {
                        result = statement->Step();
                    }
                    validResult = true;
                }

                if (result == Statement::SqlError)
                {
                    AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                    return false;
                }

                return validResult;
            }

            bool GetProductResultSimple(const char* name, Statement* statement, AssetDatabaseConnection::productHandler handler)
            {
                return GetProductResult(name, statement, handler);
            }

            bool GetProductResult(const char* callName, Statement* statement, AssetDatabaseConnection::productHandler handler, AZ::Uuid builderGuid, const char* jobKey, AssetSystem::JobStatus status)
            {
                AZ_UNUSED(callName); // AZ_Error may be compiled out entirely in release builds.
                Statement::SqlStatus result = statement->Step();

                ProductDatabaseEntry product;
                AZStd::string savedJobKey;
                AZ::Uuid savedBuilderGuid;
                int savedJobStatus;

                auto productColumns = product.GetColumns();
                auto jobKeyColumn = MakeColumn("JobKey", savedJobKey);
                auto builderGuidColumn = MakeColumn("BuilderGuid", savedBuilderGuid);
                auto statusColumn = MakeColumn("Status", savedJobStatus);

                bool validResult = result == Statement::SqlDone;
                while (result == Statement::SqlOK)
                {
                    if ((jobKey && !jobKeyColumn.Fetch(statement))
                        || (!builderGuid.IsNull() && !builderGuidColumn.Fetch(statement))
                        || (status != AssetSystem::JobStatus::Any && !statusColumn.Fetch(statement)))
                    {
                        return false;
                    }

                    if (ResultMatchesJobCriteria(jobKey, builderGuid, status, savedJobKey, savedBuilderGuid, static_cast<AssetSystem::JobStatus>(savedJobStatus)))
                    {
                        if (!productColumns.Fetch(statement))
                        {
                            return false;
                        }

                        if (handler(product))
                        {
                            result = statement->Step();
                        }
                        else
                        {
                            result = Statement::SqlDone;
                        }
                    }
                    else
                    {
                        result = statement->Step();
                    }
                    validResult = true;
                }

                if (result == Statement::SqlError)
                {
                    AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                    return false;
                }

                return validResult;
            }

            bool GetCombinedDependencyResult(const char* callName, SQLite::Statement* statement, AssetDatabaseConnection::combinedProductDependencyHandler handler)
            {
                AZ_UNUSED(callName); // AZ_Error may be compiled out entirely in release builds.

                Statement::SqlStatus result = statement->Step();

                ProductDependencyDatabaseEntry entry;
                AZ::Uuid sourceGuid;
                AZ::s32 subId;

                auto boundColumns = entry.GetColumns();
                auto guidColumn = MakeColumn("SourceGuid", sourceGuid);
                auto subIdColumn = MakeColumn("SubID", subId);

                bool validResult = result == Statement::SqlDone;
                while (result == Statement::SqlOK)
                {
                    if (!boundColumns.Fetch(statement) || !guidColumn.Fetch(statement) || !subIdColumn.Fetch(statement))
                    {
                        return false;
                    }

                    AZ::Data::AssetId assetId(sourceGuid, subId);

                    if (handler(assetId, entry))
                    {
                        result = statement->Step();
                    }
                    else
                    {
                        result = Statement::SqlDone;
                    }
                    validResult = true;
                }

                if (result == Statement::SqlError)
                {
                    AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                    return false;
                }

                return validResult;
            }
        } // namespace Internal
    } // namespace AssetDatabase
} // namespace AZToolsFramework
