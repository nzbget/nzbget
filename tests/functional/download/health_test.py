nzbget_options = ['HealthCheck=delete']

def test_medium_health(nserv, nzbget):
	nzb_content = nzbget.load_nzb('medium.nzb')
	nzb_content = nzb_content.replace('.7z', '-does-not-exist.7z')
	hist = nzbget.download_nzb('medium-health.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'
	assert hist['DeleteStatus'] == 'HEALTH'
