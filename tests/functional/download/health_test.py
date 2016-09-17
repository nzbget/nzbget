nzbget_options = ['HealthCheck=delete']

def test_1gb_health(nserv, nzbget):
	nzb_content = nzbget.load_nzb('1gb.nzb')
	nzb_content = nzb_content.replace('1gb.7z', '1gb-does-not-exist.7z')
	hist = nzbget.download_nzb('1gb-health.nzb', nzb_content)
	assert hist['Status'] == 'FAILURE/HEALTH'
	assert hist['DeleteStatus'] == 'HEALTH'
