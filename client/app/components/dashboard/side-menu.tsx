'use client'
import * as React from 'react';
import Typography from '@mui/material/Typography';
import Box from '@mui/material/Box';
import List from '@mui/material/List';
import ListItem from '@mui/material/ListItem';
import ListItemButton from '@mui/material/ListItemButton';
import ListItemText from '@mui/material/ListItemText';
import VideoIcon from '@mui/icons-material/Movie';
import MusicIcon from '@mui/icons-material/MusicNote';
import ImageIcon from '@mui/icons-material/Image';
import ListItemAvatar from '@mui/material/ListItemAvatar';
import Avatar from '@mui/material/Avatar';

export default function SideMenu() {
  const [selectedIndex, setSelectedIndex] = React.useState(1);

  const handleListItemClick = (
    event: React.MouseEvent<HTMLDivElement, MouseEvent>,
    index: number,
  ) => {
    setSelectedIndex(index);
  };
  
  return (
    <Box sx={{ width: '100%', maxWidth: 350, maxHeight: 720, bgcolor: 'background.paper', padding: 2, borderRadius: 3}}>
      <Typography component ="h5" variant="h5" gutterBottom sx={{position: 'relative', left:'.5em'}}>
        Library
      </Typography>
      <Box sx={{ width: '100%', maxWidth: 300, bgcolor: 'primary.main', margin: 'auto', borderRadius: 2 }}>
      <nav aria-label="media categories">
        <List>
          <ListItem  alignItems="flex-start">
            <ListItemButton 
              sx={{ width: '100%', maxWidth: 300, bgcolor: 'primary.main', '&:hover': {bgcolor: '#f1cdb9'}, color: 'primary.contrastText', borderRadius: 2 }}
              selected={selectedIndex === 0}
              onClick={(event) => handleListItemClick(event, 0)}
            >
              <ListItemAvatar>
                <Avatar sx={{bgcolor: '#ea5a06'}}>
                  <VideoIcon sx={{color: 'primary.contrastText'}} />
                </Avatar>
              </ListItemAvatar>
              <ListItemText primary="Videos" />
            </ListItemButton>
          </ListItem>
          <ListItem alignItems="flex-start">
            <ListItemButton 
              sx={{ width: '100%', maxWidth: 300, bgcolor: 'primary.main', '&:hover': {bgcolor: '#ecfadb'}, color: 'primary.contrastText', borderRadius: 2 }}
              selected={selectedIndex === 1}
              onClick={(event) => handleListItemClick(event, 1)}
            >
              <ListItemAvatar>
                <Avatar sx={{bgcolor: '#abf353'}}>
                  <MusicIcon sx={{color: 'primary.contrastText'}} />
                </Avatar>
              </ListItemAvatar>
              <ListItemText primary="Music" />
            </ListItemButton>
          </ListItem>
          <ListItem alignItems="flex-start">
            <ListItemButton 
              sx={{ width: '100%', maxWidth: 300, bgcolor: 'primary.main', '&:hover': {bgcolor: '#d5f8fb'}, color: 'primary.contrastText', borderRadius: 2 }}
              selected={selectedIndex === 2}
              onClick={(event) => handleListItemClick(event, 2)}
            >
              <ListItemAvatar>
                <Avatar sx={{bgcolor: '#30deea'}}>
                  <ImageIcon sx={{color: 'primary.contrastText'}} />
                </Avatar>
              </ListItemAvatar>
              <ListItemText primary="Images" />
            </ListItemButton>
          </ListItem>
        </List>
      </nav>
      </Box>
    </Box>
  );
}